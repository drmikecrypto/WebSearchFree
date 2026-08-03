#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "wsf/wsf.hpp"

namespace py = pybind11;

PYBIND11_MODULE(wsf_native, m) {
  m.doc() = "WebSearchFree — free, keyless metasearch for AI";

  py::class_<wsf::Options>(m, "Options")
      .def(py::init<>())
      .def_readwrite("max_results", &wsf::Options::max_results)
      .def_readwrite("include_raw_content", &wsf::Options::include_raw_content)
      .def_readwrite("engines", &wsf::Options::engines)
      .def_readwrite("timeout_ms", &wsf::Options::timeout_ms)
      .def_readwrite("fetch_concurrency", &wsf::Options::fetch_concurrency);

  py::class_<wsf::Result>(m, "Result")
      .def_readonly("title", &wsf::Result::title)
      .def_readonly("url", &wsf::Result::url)
      .def_readonly("content", &wsf::Result::content)
      .def_readonly("score", &wsf::Result::score)
      .def_property_readonly("raw_content", [](const wsf::Result& r) -> py::object {
        if (r.raw_content) return py::str(*r.raw_content);
        return py::none();
      });

  py::class_<wsf::Response>(m, "Response")
      .def_readonly("query", &wsf::Response::query)
      .def_readonly("results", &wsf::Response::results)
      .def("to_dict", [](const wsf::Response& r) {
        return py::module_::import("json").attr("loads")(wsf::to_json(r).dump());
      });

  m.def(
      "search",
      [](const std::string& query, int max_results, bool include_raw_content,
         const std::vector<std::string>& engines, int timeout_ms) {
        wsf::Options opt;
        opt.max_results = max_results;
        opt.include_raw_content = include_raw_content;
        if (!engines.empty()) opt.engines = engines;
        opt.timeout_ms = timeout_ms;
        return wsf::search(query, opt);
      },
      py::arg("query"), py::arg("max_results") = 5, py::arg("include_raw_content") = false,
      py::arg("engines") = std::vector<std::string>{"ddg", "brave", "wikipedia"},
      py::arg("timeout_ms") = 8000,
      "Metasearch the public web. No API key required.");

  m.def("extract", &wsf::extract, py::arg("url"), py::arg("timeout_ms") = 8000,
        "Fetch URL and extract main content.");
}
