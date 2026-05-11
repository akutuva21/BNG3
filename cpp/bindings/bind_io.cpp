#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <fstream>
#include <string>

#include "ast/Model.hpp"
#include "engine/NetworkGenerator.hpp"
#include "io/XmlWriter.hpp"
#include "io/NetWriter.hpp"
#include "io/BnglWriter.hpp"
#include "io/SbmlWriter.hpp"
#include "io/MatlabWriter.hpp"
#include "io/LatexWriter.hpp"

namespace py = pybind11;
using namespace bng::ast;
using namespace bng::engine;
using namespace bng::io;

void bind_io(py::module_& m) {
    auto io = m.def_submodule("io", "I/O writers for various output formats");

    io.def("write_xml", [](const Model& model, const std::string& path) {
        std::string content = XmlWriter::write(model);
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open file: " + path);
        out << content;
    }, py::arg("model"), py::arg("path"),
       "Write model to BioNetGen XML format");

    io.def("write_xml_string", [](const Model& model) -> std::string {
        return XmlWriter::write(model);
    }, py::arg("model"),
       "Serialize model to BioNetGen XML string");

    io.def("write_net", [](Model& model, const GeneratedNetwork& network,
                           const std::string& path) {
        NetWriter::write(path, model, network);
    }, py::arg("model"), py::arg("network"), py::arg("path"),
       "Write generated network to .net format");

    io.def("write_bngl", [](const Model& model, const std::string& path) {
        std::string content = BnglWriter::write(model);
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open file: " + path);
        out << content;
    }, py::arg("model"), py::arg("path"),
       "Write model back to BNGL format");

    io.def("write_bngl_string", [](const Model& model) -> std::string {
        return BnglWriter::write(model);
    }, py::arg("model"),
       "Serialize model to BNGL string");

    io.def("write_sbml", [](const Model& model, const GeneratedNetwork& network,
                            const std::string& path) {
        std::string content = SbmlWriter::write(model, &network);
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open file: " + path);
        out << content;
    }, py::arg("model"), py::arg("network"), py::arg("path"),
       "Write model to SBML format");

    io.def("write_matlab", [](const Model& model, const GeneratedNetwork& network,
                              const std::string& path) {
        std::string content = MatlabWriter::write(model, network);
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open file: " + path);
        out << content;
    }, py::arg("model"), py::arg("network"), py::arg("path"),
       "Write model to MATLAB format");

    io.def("write_latex", [](const Model& model, const GeneratedNetwork& network,
                             const std::string& path) {
        std::string content = LatexWriter::write(model, network);
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open file: " + path);
        out << content;
    }, py::arg("model"), py::arg("network"), py::arg("path"),
       "Write model to LaTeX format");
}
