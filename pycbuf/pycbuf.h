#pragma once

#include <Allocator.h>
#include <Python.h>
#include <inttypes.h>

#include <unordered_map>
#include <vector>

// clang-format off
struct __attribute__((__packed__)) pycbuf_preamble {
  PyObject_HEAD
  uint32_t magic;
  uint32_t size_;
  uint64_t hash;
  char* type_name;
  char* source_name; // This can be either spraybox0 or the whole cbuf name
  double packet_timest;
  uint32_t variant;
};

// clang-format on
class CBufParserPy;
struct PyMemberDef;

struct PyTypeInfo {
  CBufParserPy* parser = nullptr;
  struct PyMemberDef* members;
  PyType_Spec* spec;
  PyObject* type;
  PyObject* list;
};

struct PyCBuf_State {
  int initialized;
  PyObject* unsupported_operation;
  std::unordered_map<uint64_t, PyTypeInfo>* info_map;
  std::vector<char*>* info_sources;
  PoolAllocator* pool;
  PyTypeObject* PyCBufReader_Type = nullptr;
  PyTypeObject* PyCBufPreamble_Type = nullptr;
};

PyCBuf_State* pycbufmodule_getstate(PyObject* module);
PyObject* pycbuf_getmodule(void);
