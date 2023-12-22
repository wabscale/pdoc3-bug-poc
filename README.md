# pdoc pybind module argument bug

## Dependencies

- make
- cmake
- reasonably modern c++ compiler (eg: g++/clang++)
- python
- pdoc<=0.10.0 installed

## Running this POC

Everything is driven from `make`

```text
$ make help
Available make targets:
make gen               # Generate cmake to _bld directory
make bld               # Build module from cmake. Module will be pdoc_bug_poc.cpython-311-x86_64-linux-gnu.so or whatever py version
make cln               # Clean build artifacts
make bug               # Produce bug
```

#### Reproducing bug

To generate, build, and run the bug you can just do:

```shell
make bug
```

## Problem

The documentation library [pdoc3](https://github.com/pdoc3/pdoc) handles some pybind generated modules incorrectly. The way that it handles pybind generated function signatures is by inserting them into a python exec. It does this because the source code signature for the functions is not available in a pybind built module.

The relevant exec call for this operation is here:

https://github.com/wabscale/pdoc/blob/3ecfbcfb658c5be9ee6ab572b63db2cb5e1c29e1/pdoc/__init__.py#L1569

```python
try:
    # Eg: def epoch(self: A, dt: DateTime) -> int: pass
    exec(f'def {string}: pass', _globals, _locals)
except SyntaxError:
    # ... 
```

When you have a class with a member function with a default argument as such:

```c++
/* ... */

class A {
public:
  int epoch( DateTime const& dt = DateTime() ) { /* ... */ }
};

/* ... */

PYBIND11_MODULE(pdoc_bug_poc, m) {
  /* ... */

  py::class_<A>(m, "A")
    .def("epoch", &A::epoch, py::arg( "dt" ) = DateTime());
}
```

Pdoc will insert this function `A::epoch` into the exec statement there to get its python signature. This is in principle reasonable. If the exec fails due to a syntax error, the signature will just be skipped. The problem arises when there is an error in the exec _that is not_ a SyntaxError. The default argument for the epoch function is a `DateTime` object of default construction. That is defined here:

```c++
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ctime>


using TimePoint = std::chrono::time_point<std::chrono::system_clock>;


class DateTime {
public:
  DateTime() : _tp( std::chrono::system_clock::now() ) {}

  [[nodiscard]] std::string const toString() const {
    auto in_time_t = std::chrono::system_clock::to_time_t( _tp );
    std::stringstream ss;
    ss << std::put_time( std::localtime( &in_time_t ), "%Y%m%d-%H%M%S-%Z" );
    return ss.str();
  }

  [[nodiscard]] TimePoint const get() const {
      return _tp;
  }

private:
  TimePoint _tp;
};

/* ... */


namespace py = pybind11;
PYBIND11_MODULE(pdoc_bug_poc, m) {
  m.doc() = "pdoc_bug_poc"; // optional module docstring

  py::class_<DateTime>(m, "DateTime")
    .def(py::init<>())
    .def("toString", &DateTime::toString)
    .def( "__repr__", []( DateTime const& x ) { return x.toString(); } )
      ;

  /* ... */
}
```


Note that here the `DateTime.__repr__` object is being overwritten here. This will return the `DateTime::toString()` string instead of the auto generated one. If it is the auto generated one, then there will in fact be a `SyntaxError`. In our implementation though, that `toString` function will return a string of the form `20231222-165422-EST`. This means that what is inserted into the exec statement will be:

```python
try:
    exec('def epoch(self: A, dt: DateTime = 20231222-165422-EST) -> int: pass', _globals, _locals)
except SyntaxError:
    # ...
```

This does not produce a `SyntaxError`. It processes a NameError. Specifically:

```text
pdoc --html --force pdoc_bug_poc
/home/jc/oss/pdoc/pdoc/cli.py:534: UserWarning: Couldn't read PEP-224 variable docstrings from <Module 'pdoc_bug_poc'>: source code not available
  modules = [pdoc.Module(module, docfilter=docfilter,
/home/jc/oss/pdoc/pdoc/cli.py:534: UserWarning: Couldn't read PEP-224 variable docstrings from <Class 'pdoc_bug_poc.DateTime'>: source code not available
  modules = [pdoc.Module(module, docfilter=docfilter,
/home/jc/oss/pdoc/pdoc/cli.py:534: UserWarning: Couldn't read PEP-224 variable docstrings from <Class 'pdoc_bug_poc.A'>: source code not available
  modules = [pdoc.Module(module, docfilter=docfilter,
def epoch(self: A, dt: DateTime = 20231222-165422-EST) -> int: pass


Traceback (most recent call last):
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 155, in _render_template
    return t.render(**config).strip()
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/template.py", line 438, in render
    return runtime._render(self, self.callable_, args, data)
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/runtime.py", line 874, in _render
    _render_context(
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/runtime.py", line 916, in _render_context
    _exec_template(inherit, lclcontext, args=args, kwargs=kwargs)
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/runtime.py", line 943, in _exec_template
    callable_(context, *args, **kwargs)
  File "/home/jc/oss/pdoc/pdoc/templates/html.mako", line 429, in render_body
    ${show_module(module)}
  File "/home/jc/oss/pdoc/pdoc/templates/html.mako", line 0, in show_module

  File "/home/jc/oss/pdoc/pdoc/templates/html.mako", line 246, in render_show_module
    ${show_func(f)}
  File "/home/jc/oss/pdoc/pdoc/templates/html.mako", line 109, in show_func
    params = ', '.join(f.params(annotate=show_type_annotations, link=link))
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1434, in params
    return self._params(self, annotate=annotate, link=link, module=self.module)
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1452, in _params
    signature = Function._signature_from_string(doc_obj)
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1570, in _signature_from_string
    exec(f'def {string}: pass', _globals, _locals)
  File "<string>", line 1, in <module>

NameError: name 'EST' is not defined

Traceback (most recent call last):
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1450, in _params
    signature = inspect.signature(doc_obj.obj)
                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.11/inspect.py", line 3280, in signature
    return Signature.from_callable(obj, follow_wrapped=follow_wrapped,
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.11/inspect.py", line 3028, in from_callable
    return _signature_from_callable(obj, sigcls=cls,
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.11/inspect.py", line 2521, in _signature_from_callable
    return _signature_from_builtin(sigcls, obj,
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.11/inspect.py", line 2328, in _signature_from_builtin
    raise ValueError("no signature found for builtin {!r}".format(func))
ValueError: no signature found for builtin <instancemethod epoch at 0x7fc6e3189f90>

During handling of the above exception, another exception occurred:

Traceback (most recent call last):
  File "/home/jc/oss/pdoc3-bug-poc/venv/bin/pdoc", line 33, in <module>
    sys.exit(load_entry_point('pdoc3', 'console_scripts', 'pdoc')())
             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc/pdoc/cli.py", line 575, in main
    recursive_write_files(module, ext='.html', **template_config)
  File "/home/jc/oss/pdoc/pdoc/cli.py", line 346, in recursive_write_files
    f.write(m.html(**kwargs))
            ^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 883, in html
    html = _render_template('/html.mako', module=self, **kwargs)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 155, in _render_template
    return t.render(**config).strip()
           ^^^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/template.py", line 438, in render
    return runtime._render(self, self.callable_, args, data)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/runtime.py", line 874, in _render
    _render_context(
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/runtime.py", line 916, in _render_context
    _exec_template(inherit, lclcontext, args=args, kwargs=kwargs)
  File "/home/jc/oss/pdoc3-bug-poc/venv/lib/python3.11/site-packages/mako/runtime.py", line 943, in _exec_template
    callable_(context, *args, **kwargs)
  File "_html_mako", line 143, in render_body
  File "_html_mako", line 45, in show_module
  File "_html_mako", line 500, in render_show_module
  File "_html_mako", line 325, in show_func
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1434, in params
    return self._params(self, annotate=annotate, link=link, module=self.module)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1452, in _params
    signature = Function._signature_from_string(doc_obj)
                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/home/jc/oss/pdoc/pdoc/__init__.py", line 1570, in _signature_from_string
    exec(f'def {string}: pass', _globals, _locals)
  File "<string>", line 1, in <module>
NameError: name 'EST' is not defined
make: *** [Makefile:26: bug] Error 1
```

## Solution

The solution to make this work as expected is to loosen the exception handling for the exec call. I would suggest:

```python
try:
    exec(f'def {string}: pass', _globals, _locals)
except Exception:
    continue
```

