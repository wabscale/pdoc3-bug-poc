#include <pybind11/pybind11.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <ctime>


using TimePoint = std::chrono::time_point<std::chrono::system_clock>;


class DateTime {
public:
    DateTime() : _tp(std::chrono::system_clock::now()) {}

    [[nodiscard]] std::string const toString() const {
        auto in_time_t = std::chrono::system_clock::to_time_t(_tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d-%H%M%S-%Z");
        return ss.str();
    }

    [[nodiscard]] TimePoint const get() const {
        return _tp;
    }

private:
    TimePoint _tp;
};


class A {
public:
    int epoch(DateTime const &dt = DateTime()) {
        return std::chrono::duration_cast<std::chrono::seconds>(dt.get().time_since_epoch()).count();
    }
};


namespace py = pybind11;
PYBIND11_MODULE(pdoc_bug_poc, m) {
    m.doc() = "pdoc_bug_poc"; // optional module docstring

    py::class_<DateTime>(m, "DateTime")
            .def(py::init<>())
            .def("toString", &DateTime::toString)
            .def("__repr__", [](DateTime const &x) { return x.toString(); });

    py::class_<A>(m, "A")
            .def("epoch", &A::epoch, py::arg("dt") = DateTime());
}
