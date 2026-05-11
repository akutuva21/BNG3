# Architecture

## Overview

BioNetGen 3 is a monorepo combining three previously separate projects:

```
User (Python API / CLI)
    │
    ▼
python/bionetgen/         ← High-level Python interface
    │
    ▼ (pybind11, in-process)
cpp/bindings/             ← pybind11 binding layer
    │
    ├──► cpp/parser/      ← ANTLR4 BNGL parser → AST
    ├──► cpp/ast/         ← Model AST (Parameters, Rules, Species, ...)
    ├──► cpp/core/        ← Graph operations (PatternGraph, Ullmann, nauty)
    ├──► cpp/engine/      ← NetworkGenerator, OdeIntegrator, SSA
    ├──► cpp/io/          ← Writers (XML, SBML, .net, MATLAB, LaTeX, ...)
    ├──► cpp/actions/     ← ActionDispatch (execute model actions)
    └──► cpp/nfsim/       ← NFSim network-free engine
```

## Data Flow

### ODE/SSA Path

```
.bngl file → ANTLR4 Parser → ast::Model
                                   │
                                   ▼
                         NetworkGenerator::generateNative()
                                   │
                                   ▼
                          GeneratedNetwork (SpeciesList + RxnList)
                                   │
                                   ▼
                         OdeIntegrator::integrate()  [CVODE / Euler / RK4 / SSA]
                                   │
                                   ▼
                              OdeResult → numpy arrays (via pybind11)
```

### Network-Free Path

```
.bngl file → ANTLR4 Parser → ast::Model
                                   │
                                   ▼
                         XmlWriter::write() → XML string
                                   │
                                   ▼
                         NFinput::initializeFromXML() → NFcore::System
                                   │
                                   ▼
                         System::sim() [Gillespie SSA on molecule instances]
                                   │
                                   ▼
                              Observable counts → numpy arrays
```

## C++ Libraries

| Library | Contents | Dependencies |
|---------|----------|--------------|
| `nauty` | Graph canonicalization (C) | None |
| `bng_core` | PatternGraph, Node, State, Ullmann | nauty |
| `bng_ast` | Model, Parameter, ReactionRule, Observable, ... | bng_core, ANTLR4 |
| `bng_parser` | ANTLR4 lexer/parser, BNGAstVisitor | bng_ast, ANTLR4 |
| `bng_engine` | NetworkGenerator, OdeIntegrator, I/O writers, Actions | bng_parser, SUNDIALS |
| `nfsim_core` | NFcore, NFinput, NFreactions, NFfunction | TinyXML, muParser |
| `bionetgen_core` | Interface library linking all above | All |

## External Dependencies

All fetched via CMake FetchContent and statically linked:

- **ANTLR4 C++ Runtime 4.13.1** — Parser generator runtime (~2MB)
- **SUNDIALS 7.6.0** (CVODE only) — ODE solver
- **Catch2 3.4.0** — C++ test framework
- **pybind11 2.13.6** — Python ↔ C++ bindings

## Build System

- **CMake 3.14+** — C++ build configuration
- **scikit-build-core** — Python packaging with CMake integration
- **pybind11** — Generates `_bionetgen_cpp` Python extension module

## Python Package Structure

```
python/bionetgen/
├── __init__.py         # Public API: load(), run()
├── model.py            # BioNetGenModel (wraps C++ Model)
├── result.py           # SimResult (numpy-based results)
├── cli.py              # Click CLI (bionetgen command)
├── compat/             # Legacy backward-compatibility
│   └── legacy_runner.py  # Perl subprocess fallback
├── atomizer/           # SBML→BNGL converter (pure Python)
├── core/               # Legacy PyBioNetGen core (retained)
├── modelapi/           # Legacy model API (retained)
├── simulator/          # Legacy simulator wrappers (retained)
└── network/            # Network representation (retained)
```

## NFSim Integration Strategy

NFSim is integrated via an XML bridge rather than direct data model unification:

1. BNG C++ parses BNGL → `ast::Model`
2. `XmlWriter::write()` serializes to BNG-XML string
3. XML written to temp file
4. `NFinput::initializeFromXML()` reads XML → `NFcore::System`
5. `System::sim()` runs network-free simulation
6. Observable values extracted after simulation

This approach was chosen because:
- Both sides are extensively tested against the XML format
- Avoids the complex task of unifying two different data models
- Can be transparently replaced with direct bridge later (Phase 3)
- Serialization overhead is negligible vs simulation time

## Memory Management

- C++ `Model` is heap-allocated, owned by Python via `std::unique_ptr` + pybind11 holder
- `GeneratedNetwork` is returned by value (moved)
- `OdeResult` vectors are copied into numpy arrays at the binding boundary
- NFSim `System*` is explicitly deleted after simulation
- GIL is released during all long-running C++ operations
