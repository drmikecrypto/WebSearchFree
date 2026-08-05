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
      .def_readwrite("fetch_concurrency", &wsf::Options::fetch_concurrency)
      .def_readwrite("searx_url", &wsf::Options::searx_url);

  py::class_<wsf::Result>(m, "Result")
      .def_readonly("title", &wsf::Result::title)
      .def_readonly("url", &wsf::Result::url)
      .def_readonly("content", &wsf::Result::content)
      .def_readonly("score", &wsf::Result::score)
      .def_property_readonly("raw_content", [](const wsf::Result& r) -> py::object {
        if (r.raw_content) return py::str(*r.raw_content);
        return py::none();
      });

  py::class_<wsf::EngineStatus>(m, "EngineStatus")
      .def_readonly("name", &wsf::EngineStatus::name)
      .def_readonly("ok", &wsf::EngineStatus::ok)
      .def_readonly("hit_count", &wsf::EngineStatus::hit_count)
      .def_readonly("error", &wsf::EngineStatus::error);

  py::class_<wsf::ExtractResult>(m, "ExtractResult")
      .def_readonly("url", &wsf::ExtractResult::url)
      .def_readonly("raw_content", &wsf::ExtractResult::raw_content)
      .def_readonly("status", &wsf::ExtractResult::status)
      .def_readonly("error", &wsf::ExtractResult::error);

  py::class_<wsf::Response>(m, "Response")
      .def_readonly("query", &wsf::Response::query)
      .def_readonly("results", &wsf::Response::results)
      .def_readonly("engines", &wsf::Response::engines)
      .def_readonly("warnings", &wsf::Response::warnings)
      .def_readonly("response_time", &wsf::Response::response_time)
      .def("to_dict", [](const wsf::Response& r) {
        return py::module_::import("json").attr("loads")(wsf::to_json(r).dump());
      });

  m.def(
      "search",
      [](const std::string& query, int max_results, bool include_raw_content,
         const std::vector<std::string>& engines, int timeout_ms, const std::string& searx_url) {
        wsf::Options opt;
        opt.max_results = max_results;
        opt.include_raw_content = include_raw_content;
        if (!engines.empty()) opt.engines = engines;
        opt.timeout_ms = timeout_ms;
        opt.searx_url = searx_url;
        return wsf::search(query, opt);
      },
      py::arg("query"), py::arg("max_results") = 5, py::arg("include_raw_content") = false,
      py::arg("engines") = std::vector<std::string>{"ddg", "brave", "wikipedia"},
      py::arg("timeout_ms") = 8000, py::arg("searx_url") = "",
      "Metasearch the public web. No API key required.");

  m.def("extract", &wsf::extract, py::arg("url"), py::arg("timeout_ms") = 8000,
        "Fetch URL and extract main content (plain string).");
  m.def("extract_one", &wsf::extract_one, py::arg("url"), py::arg("timeout_ms") = 8000,
        "Fetch URL and extract with structured status.");
  m.def(
      "extract_many",
      [](const std::vector<std::string>& urls, int timeout_ms, int concurrency) {
        return wsf::extract_many(urls, timeout_ms, concurrency);
      },
      py::arg("urls"), py::arg("timeout_ms") = 8000, py::arg("concurrency") = 4,
      "Extract multiple URLs in parallel.");
}
