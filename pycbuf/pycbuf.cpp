// Main file of pycbuf module, providing CBufReader class.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include "cbuf_reader_python.h"
#include "pycbuf.h"

static PyObject* PyCBuf_getunsupportedoperation(void);
static PyObject* pycbuf_cbufreader_create_impl(PyObject* self, PyObject* args, PyObject* kwargs);

// clang-format off
typedef struct {
  PyObject_HEAD
  CBufReaderPython* reader;
} cbufreader;
// clang-format on

// --------------------------------------------------------------------------

static bool _convert_iter(PyObject* obj, PyObject** it) {
  PyObject* tmp = PyObject_GetIter(obj);
  if (tmp == NULL) return false;

  *it = tmp;
  return true;
}

static bool _convert_optional_size(PyObject* obj, Py_ssize_t* s) {
  if (obj == Py_None) return true;

  PyObject* n = PyNumber_Index(obj);
  if (n == NULL) return false;

  Py_ssize_t tmp = PyLong_AsSsize_t(n);
  Py_DECREF(n);

  if (PyErr_Occurred()) return false;

  *s = tmp;
  return true;
}

// --------------------------------------------------------------------------

PyDoc_STRVAR(pycbuf_cbufreader_close___doc__,
             "close(self)\n"
             "--\n"
             "\n"
             "Free all memory and related informaton on the ulog.\n");

static PyObject* pycbuf_cbufreader_close_impl(cbufreader* self) {
  self->reader->close();
  Py_RETURN_NONE;
}

// --------------------------------------------------------------------------

PyDoc_STRVAR(pycbuf_cbufreader_open___doc__,
             "open(self)\n"
             "--\n"
             "\n"
             "Using the previously set ulogpath and filters, open the log for processing\n");

static PyObject* pycbuf_cbufreader_open_impl(cbufreader* self) {
  if (self->reader->isOpened()) {
    Py_RETURN_TRUE;
  }
  bool b = self->reader->openUlog();
  if (!b) {
    PyErr_SetString(PyExc_FileNotFoundError, self->reader->getErrorMessage().c_str());
    return nullptr;
  }
  return PyBool_FromLong(b);
}

// --------------------------------------------------------------------------

static PyObject* pycbuf_cbufreader___new__(PyTypeObject* type, PyObject* args, PyObject* kwds) {
  cbufreader* self;

  assert(type != NULL && type->tp_alloc != NULL);
  self = (cbufreader*)type->tp_alloc(type, 0);

  return (PyObject*)self;
}

// --------------------------------------------------------------------------

PyDoc_STRVAR(
    pycbuf_cbufreader___init____doc__,
    "\n"
    "Create a CBufReader to access ulogs. Optionally, provide:"
    "ulog_path: String with the path of the .ulog folder to process\n"
    "source_filter: String or list of strings to filter the cb file where messages came from. For "
    "example: (spraybox0). Defaults to all\n"
    "message_filter: String or list of strings of messages to filter (frameinfo). Defaults to all\n"
    "early_time: Time in seconds where to start processing messages\n"
    "late_time: Time in seconds when to stop processing messages\n"
    "try_recovery: Boolean indicating if we are ok with recovery, which can skip corrupted areas on the log\n"
    "\n");

static inline int pycbuf_cbufreader___init___impl(cbufreader* self, char* ulogpath,
                                                  const std::vector<std::string>& crfilters,
                                                  const std::vector<std::string>& cmfilters,
                                                  double early_time, double late_time, bool try_recovery) {
  /* Allow calling __init__ more than once, in that case make sure to
     release any previous object reference */
  if (self->reader != nullptr) {
    self->reader->close();
  } else {
    self->reader = new CBufReaderPython();
  }

  CBufReaderBase::Options opt;
  opt.try_recovery = try_recovery;
  self->reader->setOptions(opt);

  /* Open the reader if we have a path */
  if (ulogpath) {
    self->reader->setULogPath(ulogpath);
  }
  if (!crfilters.empty()) {
    self->reader->setSourceFilters(crfilters);
  }
  if (!cmfilters.empty()) {
    self->reader->setMessageFilter(cmfilters);
  }
  if (early_time > 0) {
    self->reader->setStartTime(early_time);
  }
  if (late_time > 0) {
    self->reader->setEndTime(late_time);
  }
  if (!self->reader->getULogPath().empty()) {
    if (!self->reader->openUlog()) {
      std::string err = "Could not open ulog " + self->reader->getULogPath() +
                        " . Message: " + self->reader->getErrorMessage();
      PyErr_SetString(PyExc_Exception, err.c_str());
      return -1;
    }
  }

  return 0;
}

//
// impl function that changes source filters in object
static inline int pycbuf_cbufreader_set_sources_impl(cbufreader* self,
                                                     const std::vector<std::string>& crfilters) {
  // additional checks
  if (self->reader != nullptr) {
    // update source filters
    self->reader->setSourceFilters(crfilters);
  }
  return 0;
}
// impl function that changes message filters in object
static inline int pycbuf_cbufreader_set_messages_impl(cbufreader* self,
                                                      const std::vector<std::string>& cmfilters) {
  // additional checks
  if (self->reader != nullptr) {
    // update message filters
    self->reader->setMessageFilter(cmfilters);
  }
  return 0;
}

static inline int pycbuf_cbufreader_set_time_impl(cbufreader* self, const std::vector<double>& timeframe) {
  if (self->reader != nullptr) {
    if (timeframe.size()) {
      // if time was given, set time filters
      self->reader->set_start_time(timeframe.at(0));
      self->reader->set_end_time(timeframe.at(1));
    } else {
      // otherwise, reset filters
      self->reader->set_start_time(-1);
      self->reader->set_end_time(-1);
    }
  }
  return 0;
}

// giving python Object, returns a cpp array of strings
static std::vector<std::string> parse_filters(PyObject* args, bool& has_error) {
  // declare actual c source filters, error message is an array that is impossible to exist in reality as a
  // filter
  std::vector<std::string> filters, error_message = {"/@@/"};
  // empty array that is going to be filled with PyArg_ParseTuple
  PyObject* source_filters = nullptr;

  if (PyArg_ParseTuple(args, "|O:CBufReader_init", &source_filters)) {
    // check that source filters is not empty
    if (source_filters != nullptr) {
      // check if source filters is a python list
      if (PyList_Check(source_filters)) {
        // no idea what lsz stands for
        ssize_t lsz = PyList_Size(source_filters);

        for (ssize_t i = 0; i < lsz; i++) {
          // get i-th item from the list
          auto o = PyList_GetItem(source_filters, i);
          // check that it is not garbage, and is parsable by Python
          if (!PyUnicode_Check(o)) {
            // return error
            PyErr_SetString(PyExc_TypeError, "Argument parse_filter has to be a string or list of strings");
            has_error = true;
            return filters;
          }
          // push it to the cpp array
          filters.push_back(PyUnicode_AsUTF8(o));
        }
      }
      // In case it is not a list, we will check if it is a string
      else if (PyUnicode_Check(source_filters)) {
        // Fromat it to string and push it to cpp array
        filters.push_back(PyUnicode_AsUTF8(source_filters));
      }
      // If it is not a list or a string, then we don't want it
      else {
        // return error
        PyErr_SetString(PyExc_TypeError, "Argument parse_filter has to be a string or list of strings");
        has_error = true;
        return filters;
      }
    }
  }
  return filters;
}

// python callable function that changes source filters
static PyObject* pycbuf_cbufreader_set_sources(PyObject* self, PyObject* args) {
  std::vector<std::string> crfilters;
  // get actual cpp filters
  bool has_error = false;
  crfilters = parse_filters(args, has_error);
  if (has_error) {
    PyErr_SetString(PyExc_TypeError, "Argument set_sources has to be a string or list of strings");
    Py_RETURN_FALSE;
  }
  int status = pycbuf_cbufreader_set_sources_impl((cbufreader*)self, crfilters);
  if (status) {
    PyErr_SetString(PyExc_TypeError, "Argument set_sources has to be a string or list of strings shsh");
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

static PyObject* pycbuf_cbufreader_set_messages(PyObject* self, PyObject* args) {
  std::vector<std::string> cmfilters;
  // get actual cpp filters
  bool has_error = false;
  cmfilters = parse_filters(args, has_error);
  if (has_error) {
    PyErr_SetString(PyExc_TypeError, "Argument set_messages has to be a string or list of strings");
    Py_RETURN_FALSE;
  }
  int status = pycbuf_cbufreader_set_messages_impl((cbufreader*)self, cmfilters);
  if (status) {
    PyErr_SetString(PyExc_TypeError, "Argument set_messages has to be a string or list of strings");
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

static PyObject* pycbuf_cbufreader_set_time(PyObject* self, PyObject* args) {
  PyObject* timeframe = nullptr;
  std::vector<double> ctimeframe;
  if (PyArg_ParseTuple(args, "|O:CBufReader_init", &timeframe)) {
    if (timeframe != nullptr) {
      // If we have an array of arguments
      if (PyList_Check(timeframe)) {
        if (PyList_Size(timeframe) == 2) {
          ctimeframe.push_back(PyFloat_AsDouble(PyList_GetItem(timeframe, 0)));
          ctimeframe.push_back(PyFloat_AsDouble(PyList_GetItem(timeframe, 1)));
        } else if (PyList_Size(timeframe) != 0) {
          // give an error if number of args is not equal 2 or 0
          PyErr_SetString(PyExc_TypeError,
                          "Argument set_time has to be an empty tuple or tuple of two floats");
          Py_RETURN_FALSE;
        }
      }
      // If we have two or zero doubles passed instead of array
      else {
        PyErr_SetString(PyExc_TypeError, "Whoops");
        Py_RETURN_FALSE;
      }
      // last option is if no arguments were passed, in this case, we want for array to be empty, so we don`t
      // do anything
    }
  }
  if (pycbuf_cbufreader_set_time_impl((cbufreader*)self, ctimeframe)) {
    PyErr_SetString(PyExc_TypeError, "Whoops");
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyDoc_STRVAR(pycbuf_cbufreader_open_memory___doc__,
             "\n"
             "Open a region of memory for cbuf message decoding. Parameters:\n"
             "filename: String with the path representing the cbuf file (arbitrary)\n"
             "bytes: Array of bytes with the cbuf contents\n"
             "\n");

static PyObject* pycbuf_cbufreader_open_memory(PyObject* self, PyObject* args) {
  char* filename;
  char* binary_array;
  Py_ssize_t binary_array_size;
  if (PyArg_ParseTuple(args, "sy#", &filename, &binary_array, &binary_array_size)) {
    CBufReaderPython* reader = reinterpret_cast<cbufreader*>(self)->reader;
    reader->openMemory(filename, binary_array, binary_array_size);
    Py_RETURN_TRUE;
  } else {
    // If any param is missing
    PyErr_SetString(PyExc_TypeError, "Usage: open_memory(filename, binary_array)");
    return Py_None;
  }
}

PyDoc_STRVAR(pycbuf_cbufreader_get_counts___doc__,
             "\n"
             "Count the number of messages in a ulog. This will use the previously set\n"
             "path and role filters, but discard other filters.\n"
             "\n");

static PyObject* pycbuf_cbufreader_get_counts(PyObject* self) {
  CBufReaderPython* reader = reinterpret_cast<cbufreader*>(self)->reader;
  if (!reader->isOpened()) {
    PyErr_SetString(PyExc_TypeError, "The ulog is not opened, likely due to a previous error");
    return Py_None;
  }
  std::string errstr;
  auto msgmap = reader->getMessageCounts(errstr);
  if (!errstr.empty()) {
    PyErr_SetString(PyExc_TypeError, errstr.c_str());
    return Py_None;
  }
  // Build a python dictionary from the map
  PyObject* dict = PyDict_New();
  for (auto& kv : msgmap) {
    PyObject* key = PyUnicode_FromString(kv.first.c_str());
    PyObject* value = PyLong_FromLong(kv.second);
    PyDict_SetItem(dict, key, value);
  }
  return dict;
}

static int pycbuf_cbufreader___init__(PyObject* self, PyObject* args, PyObject* kwargs) {
  int return_value = -1;
  static const char* keywords[] = {"ulog_path",  "source_filter", "role_filter",  "message_filter",
                                   "early_time", "late_time",     "try_recovery", NULL};
  char* ulogpath = nullptr;
  PyObject* rfilters = nullptr;
  PyObject* sfilters = nullptr;
  PyObject* mfilters = nullptr;
  double early_time = -1;
  double late_time = -1;
  bool try_recovery = false;

  if (PyArg_ParseTupleAndKeywords(args, kwargs, "|sOOOddp:CBufReader_init", (char**)keywords, &ulogpath,
                                  &sfilters, &rfilters, &mfilters, &early_time, &late_time, &try_recovery)) {
    std::vector<std::string> crfilter, cmfilter;
    if (sfilters != nullptr) {
      if (PyList_Check(sfilters)) {
        ssize_t lsz = PyList_Size(sfilters);
        for (ssize_t i = 0; i < lsz; i++) {
          auto* o = PyList_GetItem(sfilters, i);
          if (!PyUnicode_Check(o)) {
            PyErr_SetString(PyExc_TypeError, "Argument source_filter has to be a string or list of strings");
            return -1;
          }
          crfilter.push_back(PyUnicode_AsUTF8(o));
        }
      } else if (PyUnicode_Check(sfilters)) {
        crfilter.push_back(PyUnicode_AsUTF8(sfilters));
      } else {
        PyErr_SetString(PyExc_TypeError, "Argument source_filter has to be a string or list of strings");
        return -1;
      }
    } else if (rfilters != nullptr) {
      // This section is deprecated
      if (PyList_Check(rfilters)) {
        ssize_t lsz = PyList_Size(rfilters);
        for (ssize_t i = 0; i < lsz; i++) {
          auto* o = PyList_GetItem(rfilters, i);
          if (!PyUnicode_Check(o)) {
            PyErr_SetString(PyExc_TypeError, "Argument role_filter has to be a string or list of strings");
            return -1;
          }
          crfilter.push_back(PyUnicode_AsUTF8(o));
        }
      } else if (PyUnicode_Check(rfilters)) {
        crfilter.push_back(PyUnicode_AsUTF8(rfilters));
      } else {
        PyErr_SetString(PyExc_TypeError, "Argument role_filter has to be a string or list of strings");
        return -1;
      }
    }

    if (mfilters != nullptr) {
      if (PyList_Check(mfilters)) {
        ssize_t lsz = PyList_Size(mfilters);
        for (ssize_t i = 0; i < lsz; i++) {
          auto* o = PyList_GetItem(mfilters, i);
          if (!PyUnicode_Check(o)) {
            PyErr_SetString(PyExc_TypeError, "Argument message_filter has to be a string or list of strings");
            return -1;
          }
          cmfilter.push_back(PyUnicode_AsUTF8(o));
        }
      } else if (PyUnicode_Check(mfilters)) {
        cmfilter.push_back(PyUnicode_AsUTF8(mfilters));
      } else {
        PyErr_SetString(PyExc_TypeError, "Argument message_filter has to be a string or list of strings");
        return -1;
      }
    }

    return_value = pycbuf_cbufreader___init___impl((cbufreader*)self, ulogpath, crfilter, cmfilter,
                                                   early_time, late_time, try_recovery);
  }

  return return_value;
}

// --------------------------------------------------------------------------

PyDoc_STRVAR(pycbuf_cbufreader___enter_____doc__,
             "__enter__(self)\n"
             "--\n"
             "\n"
             "");

static PyObject* pycbuf_cbufreader___enter___impl(cbufreader* self) {
  Py_INCREF(self);
  return (PyObject*)self;
}

// --------------------------------------------------------------------------

PyDoc_STRVAR(pycbuf_cbufreader___exit_____doc__,
             "__exit__(self, exc_type, exc_value, traceback)\n"
             "--\n"
             "\n"
             "");

static PyObject* pycbuf_cbufreader___exit___impl(cbufreader* self, PyObject* exc_type, PyObject* exc_value,
                                                 PyObject* traceback) {
  if (pycbuf_cbufreader_close_impl(self) == NULL) return NULL;
  Py_RETURN_FALSE;
}

static PyObject* pycbuf_cbufreader___exit__(PyObject* self, PyObject* args, PyObject* kwargs) {
  PyObject* return_value = NULL;
  const char* keywords[] = {"exc_type", "exc_value", "traceback", nullptr};

  PyObject* exc_type = Py_None;
  PyObject* exc_value = Py_None;
  PyObject* traceback = Py_None;

  if (PyArg_ParseTupleAndKeywords(args, kwargs, "|OOO", (char**)keywords, &exc_type, &exc_value,
                                  &traceback)) {
    return_value = pycbuf_cbufreader___exit___impl((cbufreader*)self, exc_type, exc_value, traceback);
  }

  return return_value;
}

// --------------------------------------------------------------------------

static PyObject* pycbuf_cbufreader___next___impl(cbufreader* self) {
  // Return the next object in the cbuf
  if (!self->reader->isOpened()) {
    PyErr_SetString(PyExc_Exception, "Open must be called and successful before reading");
    Py_RETURN_NONE;
  }
  PyObject* m = pycbuf_getmodule();
  auto* retobj = self->reader->getMessage(m);
  if (retobj == nullptr) {
    if (PyErr_Occurred() == nullptr) {
      PyErr_SetNone(PyExc_StopIteration);
    }
    return nullptr;
  }
  return retobj;
}

// --------------------------------------------------------------------------

static PyObject* pycbuf_cbufreader___repr___impl(cbufreader* self) {
  // Status of reader (open/close)
  std::string return_value;
  if (self->reader->isOpened())
    return_value += "Open | ";
  else
    return_value += "Closed | ";

  // Source filters
  return_value += "Source Filters: ";
  std::vector<std::string> source_filters = self->reader->getSourceFilters();
  // Check if filter is empty
  if (!self->reader->getSourceFilters().size())
    return_value += "None |";
  else {
    for (const std::string& i : source_filters) {
      return_value += i;
      return_value += ", ";
    }
    return_value.pop_back();
    return_value.pop_back();
    return_value += " | ";
  }

  // Message filters
  return_value += "Message Filters: ";
  std::vector<std::string> message_filters = self->reader->getMessageFilter();
  // Check if filter is empty
  if (!message_filters.size())
    return_value += "None | ";
  else {
    for (const std::string& i : message_filters) {
      return_value += i;
      return_value += ", ";
    }
    return_value.pop_back();
    return_value.pop_back();
    return_value += " | ";
  }

  // Time filters
  return_value += "Time filters: ";
  double startTime, endTime;
  startTime = self->reader->get_start_time();
  endTime = self->reader->get_end_time();
  // check if filters are set
  if (startTime == -1 && endTime == -1)
    return_value += "None";
  else {
    return_value += "Start time " + std::to_string(startTime);
    return_value += ", End time " + std::to_string(endTime);
  }

  return PyUnicode_FromFormat(return_value.c_str());
}

// ------------------------------------------------------------------------

static void cbufreader_dealloc(cbufreader* self) {
  if (self->reader) {
    self->reader->close();
    delete self->reader;
  }
  Py_TYPE(self)->tp_free(self);
}

// clang-format off

static int
cbufreader_clear(cbufreader *self)
{
    // Py_CLEAR(self->source);
    return 0;
}

static int
cbufreader_traverse(cbufreader* self, visitproc visit, void* arg)
{
  // Py_VISIT( some PyObject )
    return 0;
}

static struct PyMemberDef pycbufreader_members[] = {
    {NULL, 0, 0, 0, NULL}  /* Sentinel */
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type"
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static struct PyMethodDef pycbufreader_methods[] = {
    {"__enter__",           (PyCFunction)                          pycbuf_cbufreader___enter___impl,         METH_NOARGS,                  pycbuf_cbufreader___enter_____doc__   },
    {"__exit__",            (PyCFunction)                          pycbuf_cbufreader___exit__,               METH_VARARGS | METH_KEYWORDS, pycbuf_cbufreader___exit_____doc__    },
    {"open",                (PyCFunction)                          pycbuf_cbufreader_open_impl,              METH_NOARGS,                  pycbuf_cbufreader_open___doc__        },
    {"close",               (PyCFunction)                          pycbuf_cbufreader_close_impl,             METH_NOARGS,                  pycbuf_cbufreader_close___doc__       },
    {"get_message",         (PyCFunction)                          pycbuf_cbufreader___next___impl,          METH_NOARGS,                  nullptr                               },
    {"create",              (PyCFunction)(PyCFunctionWithKeywords) pycbuf_cbufreader_create_impl,            METH_VARARGS | METH_KEYWORDS, pycbuf_cbufreader___init____doc__     },
    // new
    {"set_sources",         (PyCFunction)                          pycbuf_cbufreader_set_sources,            METH_VARARGS,                 nullptr                               },
    {"set_messages",        (PyCFunction)                          pycbuf_cbufreader_set_messages,           METH_VARARGS,                 nullptr                               },
    {"set_time",            (PyCFunction)                          pycbuf_cbufreader_set_time,               METH_VARARGS,                 nullptr                               },
    {"open_memory",         (PyCFunction)                          pycbuf_cbufreader_open_memory,            METH_VARARGS,                 pycbuf_cbufreader_open_memory___doc__ },
    {"get_counts",          (PyCFunction)                          pycbuf_cbufreader_get_counts,             METH_NOARGS,                  pycbuf_cbufreader_get_counts___doc__  },
    //
    {NULL,                  NULL,                                                                         0,                            NULL                                },  /* sentinel */
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static PyTypeObject PyCBufReader_Type = {
    .ob_base      = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pycbuf.CBufReader",
    .tp_basicsize = sizeof(cbufreader),
    .tp_dealloc   = (destructor) cbufreader_dealloc,
    .tp_repr      = (reprfunc) pycbuf_cbufreader___repr___impl,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc       = pycbuf_cbufreader___init____doc__,
    .tp_traverse  = (traverseproc) cbufreader_traverse,
    .tp_clear     = (inquiry) cbufreader_clear,
    .tp_iter      = PyObject_SelfIter,
    .tp_iternext  = (iternextfunc) pycbuf_cbufreader___next___impl,
    .tp_methods   = pycbufreader_methods,
    .tp_members   = pycbufreader_members,
    .tp_init      = pycbuf_cbufreader___init__,
    .tp_new       = pycbuf_cbufreader___new__,
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

static struct PyMemberDef pycbufpreamble_members[] = {
    {"cbuf_magic", T_UINT, offsetof(pycbuf_preamble, magic), READONLY, NULL},
    {"cbuf_size", T_UINT, offsetof(pycbuf_preamble, size_), READONLY, NULL},
    {"cbuf_hash", T_ULONGLONG, offsetof(pycbuf_preamble, hash), READONLY, NULL},
    {"cbuf_packettime", T_DOUBLE, offsetof(pycbuf_preamble, packet_timest), READONLY, NULL},
    {"cbuf_variant", T_UINT, offsetof(pycbuf_preamble, variant), READONLY, NULL},
    {"cbuf_type_name", T_STRING, offsetof(pycbuf_preamble, type_name), READONLY, NULL},
    {"cbuf_source_name", T_STRING, offsetof(pycbuf_preamble, source_name), READONLY, NULL},
    {NULL, 0, 0, 0, NULL}  /* Sentinel */
};

static struct PyMethodDef pycbufpreamble_methods[] = {
    {NULL, NULL, 0, NULL}  /* sentinel */
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static PyTypeObject PyCBufPreamble_Type = {
    .ob_base      = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pycbuf.CBufPreamble",
    .tp_basicsize = sizeof(pycbuf_preamble),
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods   = pycbufpreamble_methods,
    .tp_members   = pycbufpreamble_members,
    .tp_new       = PyType_GenericNew,
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

// clang-format on

// --------------------------------------------------------------------------
static void Obj_Dealloc(PyObject* obj) {
  printf("PyType dealloc\n");
  return;
}

// This function is basically a way to debug some steps on python, not to be used
static PyObject* pycbuf_cbufreader_create_impl(PyObject* self, PyObject* args, PyObject* kwargs) {
  assert(Py_TYPE(self) == &PyCBufReader_Type);

  PyObject* return_value = NULL;
  // cbufreader *crs        = (cbufreader*) self;
  // char* name = nullptr;

  // static char* keywords[] = {"name", NULL};
  // if (PyArg_ParseTupleAndKeywords(args, kwargs, "|s", keywords, &name)) {
  //   return_value = PyObject_CallObject((PyObject*)&PyCBufPreamble_Type, nullptr);
  // }

  struct PyMemberDef Obj_members[] = {
      {"alpha", T_ULONG, sizeof(pycbuf_preamble), 0, NULL},
      {"beta", T_ULONG, sizeof(pycbuf_preamble) + 4, 0, NULL},
      {NULL, 0, 0, 0, NULL} /* Sentinel */
  };
  // struct PyMethodDef Obj_methods[] = {
  //   {NULL, NULL, 0, NULL} /* sentinel */
  // };

  PyType_Slot slots[] = {{Py_tp_dealloc, (void*)Obj_Dealloc},
                         // {Py_tp_methods, (void *)Obj_methods},
                         {Py_tp_members, (void*)Obj_members},
                         {Py_tp_base, &PyCBufPreamble_Type},
                         {0, nullptr}};

  PyType_Spec spec;
  char type_name[128];
  snprintf(type_name, sizeof(type_name), "pycbuf.testobj");
  spec.name = type_name;
  spec.itemsize = 0;
  spec.flags = Py_TPFLAGS_DEFAULT;
  spec.basicsize = sizeof(pycbuf_preamble) + 32;  // Add here the needed space for this type!
  spec.slots = slots;
  auto* newtype = PyType_FromSpec(&spec);
  return_value = PyObject_CallObject(newtype, nullptr);
  Py_INCREF(newtype);
  PyModule_AddObject(pycbuf_getmodule(), "testobj", newtype);
  pycbuf_preamble* pre = (pycbuf_preamble*)return_value;
  pre->magic = 5;
  pre->hash = 0xAABBCC;
  pre->size_ = 3;
  pre->packet_timest = 2000.0;
  return return_value;
}

// --- pycbuf module ---------------------------------------------------------

PyCBuf_State* pycbufmodule_getstate(PyObject* module) {
  void* state = PyModule_GetState(module);
  if (state == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "Cannot find module state");
    return nullptr;
  }
  return (PyCBuf_State*)state;
}

static int pycbufmodule_traverse(PyObject* mod, visitproc visit, void* arg) {
  PyCBuf_State* state = pycbufmodule_getstate(mod);
  if (!state || !state->initialized) return 0;
  Py_VISIT(state->unsupported_operation);
  return 0;
}

static int pycbufmodule_clear(PyObject* mod) {
  PyCBuf_State* state = pycbufmodule_getstate(mod);
  if (!state || !state->initialized) return 0;
  Py_CLEAR(state->unsupported_operation);
  printf("********** module pycbuf dealloc **********\n");
  return 0;
}

static void pycbufmodule_free(PyObject* mod) { pycbufmodule_clear(mod); }

static struct PyMethodDef pycbufmodule_methods[] = {
    {NULL, NULL, 0, NULL} /* sentinel */
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static struct PyModuleDef PyCBuf_Module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "pycbuf",
    .m_doc = NULL,
    .m_size = sizeof(PyCBuf_State),
    .m_methods = pycbufmodule_methods,
    .m_traverse = pycbufmodule_traverse,
    .m_clear = pycbufmodule_clear,
    .m_free = (freefunc)pycbufmodule_free,
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

PyObject* pycbuf_getmodule(void) { return PyState_FindModule(&PyCBuf_Module); }

PyMODINIT_FUNC PyInit_pycbuf(void) {
  PyObject* m = NULL;
  PyObject* _io = NULL;
  PyCBuf_State* state = NULL;

  /* Create the module and initialize state */
  m = PyModule_Create(&PyCBuf_Module);
  if (m == NULL) goto exit;
  state = pycbufmodule_getstate(m);
  if (!state) goto fail;
  state->initialized = 0;
  state->unsupported_operation = NULL;
  state->pool = new PoolAllocator();
  state->info_map = new std::unordered_map<uint64_t, PyTypeInfo>();
  state->info_sources = new std::vector<char*>();

  /* Add the `Cursor` class to the module */
  if (PyType_Ready(&PyCBufReader_Type) < 0) goto fail;
  if (PyModule_AddObject(m, "CBufReader", (PyObject*)&PyCBufReader_Type) < 0) goto fail;

  if (PyType_Ready(&PyCBufPreamble_Type) < 0) goto fail;
  if (PyModule_AddObject(m, "CBufPreamble", (PyObject*)&PyCBufPreamble_Type) < 0) goto fail;

  state->PyCBufPreamble_Type = &PyCBufPreamble_Type;
  state->PyCBufReader_Type = &PyCBufReader_Type;

  /* Import the _io module and get the `UnsupportedOperation` exception */
  _io = PyImport_ImportModule("_io");
  if (_io == NULL) goto fail;
  state->unsupported_operation = PyObject_GetAttrString(_io, "UnsupportedOperation");
  if (state->unsupported_operation == NULL) goto fail;
  if (PyModule_AddObject(m, "UnsupportedOperation", state->unsupported_operation) < 0) goto fail;

  /* Increate the reference count for classes stored in the module */
  Py_INCREF(&PyCBufReader_Type);
  Py_INCREF(state->unsupported_operation);
  Py_INCREF(&PyCBufPreamble_Type);

exit:
  return m;

fail:
  Py_DECREF(m);
  Py_XDECREF(&PyCBufReader_Type);
  Py_XDECREF(state->unsupported_operation);
  Py_XDECREF(&PyCBufPreamble_Type);
  return NULL;
}
