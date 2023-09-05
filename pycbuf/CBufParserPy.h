#pragma once

#include <CBufParser.h>  // in cbuf/include
#include <Python.h>
#include <stdint.h>

struct PyCBuf_State;

class CBufParserPy : public CBufParser {
  double current_timestamp_;
  char* source_cbuf_file_ = nullptr;
  uint32_t magic_;
  PyTypeObject* GetPyTypeFromCBuf(uint64_t hash, ast_struct* st, PyObject* m, PyCBuf_State* state);
  bool FillPyObjectInternal(uint64_t hash, ast_struct* st, PyObject* m, PyObject*& obj, PyCBuf_State* state);

public:
  CBufParserPy();
  ~CBufParserPy();

  // This function will parse a message defined by hash (needs to have been parsed in metadata already), from
  // the buffer, and fill the python object obj with its data
  unsigned int FillPyObject(uint64_t hash, const char* st_name, const unsigned char* buffer, size_t buf_size,
                            const char* source_cbuf_file, PyObject* m, PyObject*& obj);
};
