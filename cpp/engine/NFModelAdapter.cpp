#include "nfsim/NFinput/NFinput.hh"

#include <algorithm>
#include <cctype>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast/Model.hpp"
#include "ast/Observable.hpp"
#include "ast/ReactionRule.hpp"
#include "parser/PatternGraphBuilder.hpp"

#include "generated/BNGLexer.h"
#include "generated/BNGParser.h"

#include "antlr4-runtime.h"

namespace {

using bng::ast::Expression;
using bng::ast::Model;
using bng::ast::ReactionRule;
using bng::ast::SpeciesGraph;

bool isBondNode(const BNGcore::Node& node) {
    return node.get_type() < BNGcore::LINK_NODE_TYPE;
}

bool isComponentNode(const BNGcore::Node& node) {
    return node.get_type() < BNGcore::COMPONENT_NODE_TYPE;
}

bool isMoleculeNode(const BNGcore::Node& node) {
    return !isBondNode(node) && !isComponentNode(node);
}

std::string stripLeadingTilde(const std::string& text) {
    if (!text.empty() && text.front() == '~') {
        return text.substr(1);
    }
    return text;
}

std::string stateToken(const BNGcore::Node& node) {
    return stripLeadingTilde(node.get_state().get_BNG2_string());
}

std::string bondToken(const BNGcore::Node& node) {
    return node.get_state().get_BNG2_string();
}

class ThrowingErrorListener : public antlr4::BaseErrorListener {
public:
    void syntaxError(antlr4::Recognizer*,
                     antlr4::Token*,
                     size_t line,
                     size_t charPositionInLine,
                     const std::string& msg,
                     std::exception_ptr) override {
        throw std::runtime_error("BNGL parse error at " + std::to_string(line) + ":" +
                                 std::to_string(charPositionInLine) + ": " + msg);
    }
};

SpeciesGraph parseSpeciesGraphFromString(const std::string& text, Model& model, bool treatUnspecifiedBondAsWildcard) {
    antlr4::ANTLRInputStream input(text);
    BNGLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    BNGParser parser(&tokens);

    ThrowingErrorListener listener;
    parser.removeErrorListeners();
    parser.addErrorListener(&listener);

    auto* ctx = parser.species_def();
    auto graph = bng::parser::buildPatternGraph(ctx, model, treatUnspecifiedBondAsWildcard);
    SpeciesGraph sg(std::move(graph));
    sg.setCompartment(bng::parser::extractSpeciesCompartment(ctx));
    sg.setCompartmentIsPrefix(bng::parser::isSpeciesCompartmentPrefix(ctx));
    return sg;
}

struct IndexedPattern {
    std::vector<BNGcore::Node*> moleculeNodes;
    std::vector<std::vector<BNGcore::Node*>> componentNodesByMolecule;
    std::unordered_map<BNGcore::Node*, std::pair<std::size_t, std::size_t>> componentIndexByNode;
};

IndexedPattern indexPatternGraph(const BNGcore::PatternGraph& graph) {
    IndexedPattern indexed;

    for (auto nodeIter = graph.begin(); nodeIter != graph.end(); ++nodeIter) {
        if (!isMoleculeNode(**nodeIter)) {
            continue;
        }

        auto* moleculeNode = *nodeIter;
        indexed.moleculeNodes.push_back(moleculeNode);

        std::vector<BNGcore::Node*> components;
        std::size_t componentIndex = 0;
        for (auto edge = moleculeNode->edges_out_begin(); edge != moleculeNode->edges_out_end(); ++edge) {
            if (!isComponentNode(**edge)) {
                continue;
            }
            components.push_back(*edge);
            indexed.componentIndexByNode[*edge] = {indexed.moleculeNodes.size() - 1, componentIndex++};
        }
        indexed.componentNodesByMolecule.push_back(std::move(components));
    }

    return indexed;
}

struct TemplatePattern {
    std::vector<NFcore::TemplateMolecule*> templatesByMoleculeIndex;
    std::vector<std::vector<std::string>> componentNamesByMolecule;
    NFcore::TemplateMolecule* head = nullptr;
};

TemplatePattern buildTemplatePattern(const SpeciesGraph& pattern, NFcore::System* system) {
    TemplatePattern result;

    const auto indexed = indexPatternGraph(pattern.getGraph());
    const auto& graph = pattern.getGraph();

    result.templatesByMoleculeIndex.reserve(indexed.moleculeNodes.size());
    result.componentNamesByMolecule.reserve(indexed.moleculeNodes.size());

    std::unordered_map<BNGcore::Node*, NFcore::TemplateMolecule*> templateByMoleculeNode;
    std::unordered_map<BNGcore::Node*, std::pair<NFcore::TemplateMolecule*, std::string>> templateByComponentNode;

    // Pass 1: create TemplateMolecules and add local component constraints (state, empty/occupied).
    for (std::size_t mi = 0; mi < indexed.moleculeNodes.size(); ++mi) {
        auto* moleculeNode = indexed.moleculeNodes[mi];
        const std::string molName = moleculeNode->get_type().get_type_name();

        NFcore::MoleculeType* mt = system->getMoleculeTypeByName(molName);
        if (mt == nullptr) {
            throw std::runtime_error("NFsim initializeFromModel: unknown MoleculeType '" + molName + "'");
        }

        auto* tm = new NFcore::TemplateMolecule(mt);

        const auto& molComp = moleculeNode->get_compartment();
        if (!molComp.empty()) {
            auto* comp = system->getCompartment(molComp);
            if (comp == nullptr) {
                throw std::runtime_error("NFsim initializeFromModel: unknown compartment '" + molComp + "' on molecule '" + molName + "'");
            }
            tm->setCompartment(comp);
        }

        templateByMoleculeNode[moleculeNode] = tm;
        result.templatesByMoleculeIndex.push_back(tm);

        std::vector<std::string> compNames;
        compNames.reserve(indexed.componentNodesByMolecule[mi].size());

        for (auto* componentNode : indexed.componentNodesByMolecule[mi]) {
            const std::string compName = componentNode->get_type().get_type_name();
            compNames.push_back(compName);
            templateByComponentNode[componentNode] = {tm, compName};

            const std::string st = stateToken(*componentNode);
            if (!st.empty() && st != BNGcore::WILDCARD_STRING) {
                tm->addComponentConstraint(compName, st);
            }

            // Bond/site occupancy constraints
            for (auto bondEdge = componentNode->edges_out_begin(); bondEdge != componentNode->edges_out_end(); ++bondEdge) {
                if (!isBondNode(**bondEdge)) {
                    continue;
                }
                const std::string bt = bondToken(**bondEdge);
                if (bt == "!-") {
                    tm->addEmptyComponent(compName);
                } else if (bt == "!+") {
                    // Defer explicit two-sided bonds to pass 2. If this is a one-sided "+",
                    // record it as an occupied (bound to something unspecified) site.
                    int nComponentParents = 0;
                    for (auto inEdge = (*bondEdge)->edges_in_begin(); inEdge != (*bondEdge)->edges_in_end(); ++inEdge) {
                        if (isComponentNode(**inEdge)) {
                            nComponentParents++;
                        }
                    }
                    if (nComponentParents == 1) {
                        tm->addBoundComponent(compName);
                    }
                } else {
                    // wildcard or unknown: no constraint
                }
            }
        }

        result.componentNamesByMolecule.push_back(std::move(compNames));
    }

    // Pass 2: explicit bonds (bond nodes with state "!+" and exactly two component endpoints)
    std::unordered_set<BNGcore::Node*> visitedBondNodes;
    for (auto nodeIter = graph.begin(); nodeIter != graph.end(); ++nodeIter) {
        if (!isBondNode(**nodeIter)) {
            continue;
        }
        auto* bondNode = *nodeIter;
        if (bondToken(*bondNode) != "!+") {
            continue;
        }
        if (!visitedBondNodes.insert(bondNode).second) {
            continue;
        }

        std::vector<BNGcore::Node*> endpoints;
        for (auto inEdge = bondNode->edges_in_begin(); inEdge != bondNode->edges_in_end(); ++inEdge) {
            if (isComponentNode(**inEdge)) {
                endpoints.push_back(*inEdge);
            }
        }
        if (endpoints.size() != 2) {
            continue;
        }

        const auto it1 = templateByComponentNode.find(endpoints[0]);
        const auto it2 = templateByComponentNode.find(endpoints[1]);
        if (it1 == templateByComponentNode.end() || it2 == templateByComponentNode.end()) {
            continue;
        }

        NFcore::TemplateMolecule::bind(
            it1->second.first, it1->second.second, "",
            it2->second.first, it2->second.second, "");
    }

    result.head = result.templatesByMoleculeIndex.empty() ? nullptr : result.templatesByMoleculeIndex.front();
    return result;
}

double evalExpressionOrThrow(const Expression& expr, const Model& model) {
    return expr.evaluate([&](const std::string& name) {
        return model.getParameters().evaluate(name);
    }, 0.0);
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

struct ObservablePatternSpec {
    std::string patternText;
    std::string relation;
    int quantity = 0;
};

ObservablePatternSpec parseObservablePatternSpec(const std::string& text) {
    // Match trailing stoichiometry constraint: <pattern><op><int>
    // Examples: X()>2, R==1
    static const std::regex trailingConstraint(R"(^(.+?)(==|>=|<=|>|<)(\d+)$)");
    std::smatch m;
    if (std::regex_match(text, m, trailingConstraint)) {
        ObservablePatternSpec spec;
        spec.patternText = m[1].str();
        spec.relation = m[2].str();
        spec.quantity = std::stoi(m[3].str());
        return spec;
    }

    ObservablePatternSpec spec;
    spec.patternText = text;
    return spec;
}

} // namespace

namespace NFinput {

System* initializeFromModel(const bng::ast::Model& model,
                            bool blockSameComplexBinding,
                            int globalMoleculeLimit,
                            bool verbose,
                            int& suggestedTraversalLimit,
                            bool evaluateComplexScopedLocalFunctions,
                            bool connectivityFlag) {
    // NOTE: This is intentionally a minimal AST→NFcore adapter that covers the
    // models used by BNG3's current NFSim in-process path. Unsupported features
    // should fail fast so callers can fall back to the XML bridge if needed.

    const std::string modelName = model.getModelName().empty() ? "nameless" : model.getModelName();
    auto* system = new NFcore::System(modelName, blockSameComplexBinding, globalMoleculeLimit);

    system->setEvaluateComplexScopedLocalFunctions(evaluateComplexScopedLocalFunctions);
    system->useConnectivityFlag(connectivityFlag);

    // Compartments
    if (!model.getCompartments().empty()) {
        std::unordered_map<std::string, NFcore::Compartment*> compartments;
        compartments.reserve(model.getCompartments().size());
        for (const auto& comp : model.getCompartments()) {
            auto* nfComp = new NFcore::Compartment(comp.getName(), comp.getDimension(), comp.getVolume(), nullptr);
            system->addCompartment(nfComp);
            compartments[comp.getName()] = nfComp;
        }
        for (const auto& comp : model.getCompartments()) {
            if (comp.getParent().empty()) {
                continue;
            }
            auto itChild = compartments.find(comp.getName());
            auto itParent = compartments.find(comp.getParent());
            if (itChild != compartments.end() && itParent != compartments.end()) {
                itChild->second->setParent(itParent->second);
            }
        }
    }

    // Parameters
    for (const auto& param : model.getParameters().all()) {
        const double value = model.getParameters().evaluate(param.getName());
        system->addParameter(param.getName(), value);
        if (verbose) {
            std::cout << "\t[param] " << param.getName() << " = " << value << "\n";
        }
    }

    // Molecule types
    for (const auto& mt : model.getMoleculeTypes()) {
        std::vector<std::string> compNames;
        std::vector<std::string> defaultStates;
        std::vector<std::vector<std::string>> possibleStates;
        std::vector<bool> isIntegerComponent;

        compNames.reserve(mt.getComponents().size());
        defaultStates.reserve(mt.getComponents().size());
        possibleStates.reserve(mt.getComponents().size());
        isIntegerComponent.reserve(mt.getComponents().size());

        for (const auto& comp : mt.getComponents()) {
            compNames.push_back(comp.name);
            possibleStates.push_back(comp.allowedStates);
            defaultStates.push_back(comp.allowedStates.empty() ? "NO_STATE" : comp.allowedStates.front());
            isIntegerComponent.push_back(false);
        }

        // MoleculeType registers itself with the System in its ctor.
        (void)new NFcore::MoleculeType(
            mt.getName(),
            compNames,
            defaultStates,
            possibleStates,
            isIntegerComponent,
            mt.isPopulation(),
            system);
    }

    // Seed species (initial molecules)
    for (const auto& seed : model.getSeedSpecies()) {
        int count = 0;
        try {
            count = static_cast<int>(evalExpressionOrThrow(seed.getAmount(), model));
        } catch (const std::exception& e) {
            delete system;
            throw std::runtime_error(std::string("NFsim initializeFromModel: failed to evaluate seed amount: ") + e.what());
        }
        if (count < 0) {
            delete system;
            throw std::runtime_error("NFsim initializeFromModel: negative seed amount for '" + seed.getPattern() + "'");
        }

        const auto indexed = indexPatternGraph(seed.getGraph());
        if (indexed.moleculeNodes.empty()) {
            continue;
        }

        // Fixed species (BNGL `$`): only supported for single-molecule seeds in NFsim.
        if (seed.isConstant()) {
            if (indexed.moleculeNodes.size() != 1) {
                delete system;
                throw std::runtime_error("NFsim initializeFromModel: fixed multi-molecule seed species not supported: '" + seed.getPattern() + "'");
            }
        }

        // For population species, NFsim expects a single molecule instance with population count.
        bool isPopulation = false;
        {
            const std::string molName = indexed.moleculeNodes.front()->get_type().get_type_name();
            auto* mt = system->getMoleculeTypeByName(molName);
            isPopulation = (mt != nullptr && mt->isPopulationType());
        }

        const int nCopies = isPopulation ? 1 : count;
        std::vector<std::vector<NFcore::Molecule*>> moleculesByMolIndex(indexed.moleculeNodes.size());
        std::unordered_map<BNGcore::Node*, std::pair<std::size_t, std::string>> componentToMolAndName;

        for (std::size_t mi = 0; mi < indexed.moleculeNodes.size(); ++mi) {
            auto* moleculeNode = indexed.moleculeNodes[mi];
            const std::string molName = moleculeNode->get_type().get_type_name();
            auto* mt = system->getMoleculeTypeByName(molName);
            if (mt == nullptr) {
                delete system;
                throw std::runtime_error("NFsim initializeFromModel: unknown MoleculeType '" + molName + "' in seed species");
            }

            NFcore::Compartment* compartment = nullptr;
            if (!moleculeNode->get_compartment().empty()) {
                compartment = system->getCompartment(moleculeNode->get_compartment());
            } else if (!seed.getCompartment().empty()) {
                compartment = system->getCompartment(seed.getCompartment());
            } else {
                compartment = system->getDefaultCompartment();
            }

            moleculesByMolIndex[mi].reserve(static_cast<std::size_t>(std::max(0, nCopies)));
            for (int copy = 0; copy < nCopies; ++copy) {
                auto* mol = mt->genDefaultMolecule(compartment);

                // Apply component states
                for (auto* componentNode : indexed.componentNodesByMolecule[mi]) {
                    const std::string compName = componentNode->get_type().get_type_name();
                    componentToMolAndName[componentNode] = {mi, compName};

                    const std::string st = stateToken(*componentNode);
                    if (!st.empty() && st != BNGcore::WILDCARD_STRING) {
                        mol->setComponentState(compName, mt->getStateValueFromName(mt->getCompIndexFromName(compName), st));
                    }
                }

                if (isPopulation) {
                    mol->setPopulation(count);
                }

                moleculesByMolIndex[mi].push_back(mol);
            }

            if (seed.isConstant()) {
                mt->setFixed(true, count, compartment);
            }
        }

        if (!isPopulation) {
            // Bind explicit bonds within each copy
            for (auto nodeIter = seed.getGraph().begin(); nodeIter != seed.getGraph().end(); ++nodeIter) {
                if (!isBondNode(**nodeIter)) {
                    continue;
                }
                auto* bondNode = *nodeIter;
                if (bondToken(*bondNode) != "!+") {
                    continue;
                }
                std::vector<BNGcore::Node*> endpoints;
                for (auto inEdge = bondNode->edges_in_begin(); inEdge != bondNode->edges_in_end(); ++inEdge) {
                    if (isComponentNode(**inEdge)) {
                        endpoints.push_back(*inEdge);
                    }
                }
                if (endpoints.size() != 2) {
                    continue;
                }

                const auto a = componentToMolAndName.find(endpoints[0]);
                const auto b = componentToMolAndName.find(endpoints[1]);
                if (a == componentToMolAndName.end() || b == componentToMolAndName.end()) {
                    continue;
                }

                for (int copy = 0; copy < count; ++copy) {
                    NFcore::Molecule::bind(
                        moleculesByMolIndex[a->second.first].at(copy),
                        a->second.second,
                        moleculesByMolIndex[b->second.first].at(copy),
                        b->second.second);
                }
            }
        }
    }

    // Observables
    for (const auto& obs : model.getObservables()) {
        const auto typeLower = lowercase(obs.getType());
        const bool isSpecies = (typeLower == "species");

        std::vector<NFcore::TemplateMolecule*> templates;
        std::vector<std::string> stochRelation;
        std::vector<int> stochQuantity;

        for (const auto& patternText : obs.getPatterns()) {
            const auto spec = parseObservablePatternSpec(patternText);

            // Only parse species_def-based observable patterns here.
            // For pure stoichiometry forms (e.g., "R==1"), keep the pattern string as-is
            // and let NFsim handle it via the stochastic constraints interface.
            SpeciesGraph sg = parseSpeciesGraphFromString(spec.patternText, const_cast<Model&>(model), true);
            auto tp = buildTemplatePattern(sg, system);
            if (tp.head != nullptr) {
                templates.push_back(tp.head);
            }

            stochRelation.push_back(spec.relation.empty() ? std::string() : spec.relation);
            stochQuantity.push_back(spec.quantity);
        }

        NFcore::Observable* nfObs = nullptr;
        if (isSpecies) {
            nfObs = new NFcore::SpeciesObservable(obs.getName(), templates, stochRelation, stochQuantity);
        } else {
            // Default to Molecules observable
            if (std::any_of(stochRelation.begin(), stochRelation.end(), [](const std::string& s) { return !s.empty(); })) {
                nfObs = new NFcore::MoleculesObservable(obs.getName(), templates, stochRelation, stochQuantity);
            } else {
                nfObs = new NFcore::MoleculesObservable(obs.getName(), templates);
            }
        }

        system->addObservableForOutput(nfObs);
    }

    // Reaction rules
    for (const auto& rule : model.getReactionRules()) {
        auto addRuleInstance = [&](const ReactionRule& rr, const Expression& rateExpr, const std::string& name) {
            // Build a standalone ReactionRule instance so we can call initialize()
            // without mutating the caller's const rule.
            ReactionRule local(
                rr.getRuleName(),
                rr.getLabel(),
                rr.getReactants(),
                rr.getProducts(),
                rr.getRates(),
                rr.getModifiers(),
                rr.isBidirectional(),
                rr.getReactantPatterns(),
                rr.getProductPatterns());
            if (rr.hasScopePrefix()) {
                local.setHasScopePrefix(true);
            }
            local.initialize();

            // Build reactant TemplateMolecules
            std::vector<TemplatePattern> reactantPatterns;
            reactantPatterns.reserve(local.getReactantPatterns().size());

            std::vector<NFcore::TemplateMolecule*> reactantHeads;
            reactantHeads.reserve(local.getReactantPatterns().size());

            for (const auto& patt : local.getReactantPatterns()) {
                auto tp = buildTemplatePattern(patt, system);
                if (tp.head == nullptr) {
                    throw std::runtime_error("NFsim initializeFromModel: empty reactant pattern in rule '" + name + "'");
                }
                reactantHeads.push_back(tp.head);
                reactantPatterns.push_back(std::move(tp));
            }

            // Map operations to NFsim transformations.
            auto* ts = new NFcore::TransformationSet(reactantHeads);

            std::unordered_set<std::size_t> speciesRemovalPatterns;
            for (const auto& op : local.getOperations()) {
                switch (op.type) {
                case ReactionRule::TransformOp::Type::ChangeState: {
                    const auto& tp = reactantPatterns.at(op.source.patternIndex);
                    const auto* tm = tp.templatesByMoleculeIndex.at(op.source.moleculeIndex);
                    const std::string& compName = tp.componentNamesByMolecule.at(op.source.moleculeIndex).at(op.source.componentIndex);
                    if (!op.newState.empty()) {
                        ts->addStateChangeTransform(const_cast<NFcore::TemplateMolecule*>(tm), compName, op.newState);
                    }
                    break;
                }
                case ReactionRule::TransformOp::Type::AddBond: {
                    const auto& tp1 = reactantPatterns.at(op.source.patternIndex);
                    const auto& tp2 = reactantPatterns.at(op.partner.patternIndex);
                    auto* tm1 = tp1.templatesByMoleculeIndex.at(op.source.moleculeIndex);
                    auto* tm2 = tp2.templatesByMoleculeIndex.at(op.partner.moleculeIndex);
                    const std::string& c1 = tp1.componentNamesByMolecule.at(op.source.moleculeIndex).at(op.source.componentIndex);
                    const std::string& c2 = tp2.componentNamesByMolecule.at(op.partner.moleculeIndex).at(op.partner.componentIndex);
                    ts->addBindingTransform(tm1, c1, tm2, c2);
                    break;
                }
                case ReactionRule::TransformOp::Type::DeleteBond: {
                    const auto& tp1 = reactantPatterns.at(op.source.patternIndex);
                    const auto& tp2 = reactantPatterns.at(op.partner.patternIndex);
                    auto* tm1 = tp1.templatesByMoleculeIndex.at(op.source.moleculeIndex);
                    auto* tm2 = tp2.templatesByMoleculeIndex.at(op.partner.moleculeIndex);
                    const std::string& c1 = tp1.componentNamesByMolecule.at(op.source.moleculeIndex).at(op.source.componentIndex);
                    const std::string& c2 = tp2.componentNamesByMolecule.at(op.partner.moleculeIndex).at(op.partner.componentIndex);
                    ts->addUnbindingTransform(tm1, c1, tm2, c2);
                    break;
                }
                case ReactionRule::TransformOp::Type::DeleteMolecule: {
                    // BNG3 represents full degradation (->0) as DeleteMolecule for each molecule
                    // in the reactant pattern. COMPLETE_SPECIES_REMOVAL should only be added once.
                    if (!speciesRemovalPatterns.insert(op.patternIndex).second) {
                        break;
                    }
                    auto* tm = reactantPatterns.at(op.patternIndex).templatesByMoleculeIndex.at(0);
                    ts->addDeleteMolecule(tm, NFcore::TransformationFactory::COMPLETE_SPECIES_REMOVAL);
                    break;
                }
                case ReactionRule::TransformOp::Type::AddMolecule:
                    throw std::runtime_error("NFsim initializeFromModel: AddMolecule not yet supported for rule '" + name + "'");
                }
            }

            ts->finalize();

            double baseRate = 0.0;
            try {
                baseRate = evalExpressionOrThrow(rateExpr, model);
            } catch (const std::exception& e) {
                delete ts;
                throw std::runtime_error(std::string("NFsim initializeFromModel: failed to evaluate rate law: ") + e.what());
            }

            const std::string rateText = rateExpr.toString();
            const std::string baseRateParamName =
                model.getParameters().contains(rateText) ? rateText : std::string();

            auto* rxn = new NFcore::ReactionClass(name, baseRate, baseRateParamName, ts, system);
            system->addReaction(rxn);
        };

        const std::string baseName = rule.getRuleName().empty() ? "rule" : rule.getRuleName();
        if (rule.getRates().empty()) {
            continue;
        }

        if (!rule.isBidirectional()) {
            addRuleInstance(rule, rule.getRates().front(), baseName);
        } else {
            // Forward
            addRuleInstance(rule, rule.getRates().front(), baseName);

            // Reverse: swap reactant/product patterns and use the second rate.
            if (rule.getRates().size() >= 2) {
                ReactionRule reverseRule(
                    baseName + "(reverse)",
                    rule.getLabel(),
                    rule.getProducts(),
                    rule.getReactants(),
                    {rule.getRates()[1]},
                    rule.getModifiers(),
                    false,
                    rule.getProductPatterns(),
                    rule.getReactantPatterns());
                addRuleInstance(reverseRule, reverseRule.getRates().front(), reverseRule.getRuleName());
            }
        }
    }

    if (verbose) {
        std::cout << "NFsim initializeFromModel: built system '" << system->getName()
                  << "' (molTypes=" << system->getNumOfMoleculeTypes()
                  << ", reactions=" << system->getAllReactions().size()
                  << ", observables=" << system->getObsToOutput().size()
                  << ", molecules=" << system->getNumOfMolecules()
                  << ")\n";
    }

    (void)suggestedTraversalLimit; // computed by XML path; keep signature parity
    return system;
}

} // namespace NFinput
