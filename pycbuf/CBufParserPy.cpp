#include "CBufParserPy.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <structmember.h>

#include <cmath>

#include "Interp.h"
#include "Parser.h"
#include "StdStringBuffer.h"
#include "SymbolTable.h"
#include "cbuf_preamble.h"
#include "pycbuf.h"

const char* get_str_for_elem_type(ElementType t);

PyObject* BuildPyObjectFromNumber(s8 n) { return PyLong_FromLong(n); }
PyObject* BuildPyObjectFromNumber(s16 n) { return PyLong_FromLong(n); }
PyObject* BuildPyObjectFromNumber(s32 n) { return PyLong_FromLong(n); }

PyObject* BuildPyObjectFromNumber(u8 n) { return PyLong_FromUnsignedLong(n); }
PyObject* BuildPyObjectFromNumber(u16 n) { return PyLong_FromUnsignedLong(n); }
PyObject* BuildPyObjectFromNumber(u32 n) { return PyLong_FromUnsignedLong(n); }

PyObject* BuildPyObjectFromNumber(s64 n) { return PyLong_FromLongLong(n); }
PyObject* BuildPyObjectFromNumber(u64 n) { return PyLong_FromUnsignedLongLong(n); }

PyObject* BuildPyObjectFromNumber(f32 n) { return PyFloat_FromDouble(n); }
PyObject* BuildPyObjectFromNumber(f64 n) { return PyFloat_FromDouble(n); }

template <class T>
bool process_element_py(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size, PyObject* m,
                        PyObject* obj, PyMemberDef* mdef) {
  T val;
  u8* byte_ptr = (u8*)obj;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }
    if (elem->is_compact_array && array_size > elem->array_suffix->size) {
      return false;
    }

    // Create Python Array/List
    PyObject* list = PyList_New(array_size);
    if (list == nullptr) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      val = *(const T*)bin_buffer;
      bin_buffer += sizeof(val);
      buf_size -= sizeof(val);
      PyObject* innerobj = nullptr;
      if (mdef->type == T_BOOL) {
        if (sizeof(T) != 1) {
          PyErr_Format(PyExc_ValueError, "Invalid size for bool (%d)", sizeof(T));
          return false;
        }
        innerobj = PyBool_FromLong((uint8_t)val);
      } else {
        innerobj = BuildPyObjectFromNumber(val);
      }
      if (innerobj == nullptr) {
        PyErr_Format(PyExc_ValueError, "Invalid value for %s", mdef->name);
        return false;
      }
      PyList_SET_ITEM(list, i, innerobj);
    }
    *(PyObject**)(byte_ptr + mdef->offset) = list;
  } else {
    // This is a single element, not an array
    val = *(const T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    *(T*)(byte_ptr + mdef->offset) = val;
  }
  return true;
}

static bool process_element_string_py(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                      PyObject* m, PyObject* obj, PyMemberDef* mdef) {
  const char* str;
  u8* byte_ptr = (u8*)obj;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    if (elem->is_compact_array && array_size > elem->array_suffix->size) {
      return false;
    }

    // Create Python Array/List
    PyObject* list = PyList_New(array_size);
    if (list == nullptr) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      str = (char*)bin_buffer;
      bin_buffer += str_size;
      buf_size -= str_size;

      auto listobj = PyUnicode_FromStringAndSize(str, str_size);
      if (listobj == nullptr) {
        PyErr_Clear();
        listobj = PyUnicode_FromString("<invalid>");
      }
      PyList_SET_ITEM(list, i, listobj);
    }
    *(PyObject**)(byte_ptr + mdef->offset) = list;

    return true;
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(const u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  str = (const char*)bin_buffer;
  bin_buffer += str_size;
  buf_size -= str_size;

  auto listobj = PyUnicode_FromStringAndSize(str, str_size);
  if (listobj == nullptr) {
    PyErr_Clear();
    listobj = PyUnicode_FromString("<invalid>");
  }

  if (listobj == nullptr) return false;
  *(PyObject**)(byte_ptr + mdef->offset) = listobj;
  return true;
}

static bool process_element_short_string_py(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                            PyObject* m, PyObject* obj, PyMemberDef* mdef) {
  u8* byte_ptr = (u8*)obj;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    if (elem->is_compact_array && array_size > elem->array_suffix->size) {
      return false;
    }

    // Create Python Array/List
    PyObject* list = PyList_New(array_size);
    if (list == nullptr) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size = 16;
      char str[16] = {};
      strncpy(str, (const char*)bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;

      auto listobj = PyUnicode_FromStringAndSize(str, strnlen(str, str_size));
      if (listobj == nullptr) {
        return false;
      }
      PyList_SET_ITEM(list, i, listobj);
    }
    *(PyObject**)(byte_ptr + mdef->offset) = list;
    return true;
  }

  // This is s static array
  u32 str_size = 16;
  char str[16] = {};
  strncpy(str, (const char*)bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  auto listobj = PyUnicode_FromStringAndSize(str, strnlen(str, str_size));
  if (listobj == nullptr) {
    return false;
  }
  *(PyObject**)(byte_ptr + mdef->offset) = listobj;
  return true;
}

static u64 hash(const unsigned char* str) {
  u64 hash = 5381;
  int c;

  while ((c = *str++)) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

static bool compute_hash(ast_struct* st, SymbolTable* symtable) {
  StdStringBuffer buf;
  if (st->hash_computed) return true;

  buf.print("struct ");
  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buf.print_no("%s::", st->space->name);
  buf.print("%s \n", st->name);
  for (auto* elem : st->elements) {
    if (elem->array_suffix) {
      buf.print("[%" PRIu64 "] ", elem->array_suffix->size);
    }
    if (elem->type == TYPE_CUSTOM) {
      auto* enm = symtable->find_enum(elem);
      if (enm != nullptr) {
        buf.print("%s %s;\n", elem->custom_name, elem->name);
        continue;
      }
      auto* inner_st = symtable->find_struct(elem);
      if (!inner_st) {
        return false;
      }
      assert(inner_st);
      bool bret = compute_hash(inner_st, symtable);
      if (!bret) return false;
      buf.print("%" PRIX64 " %s;\n", inner_st->hash_value, elem->name);
    } else {
      buf.print("%s %s; \n", get_str_for_elem_type(elem->type), elem->name);
    }
  }

  st->hash_value = hash((const unsigned char*)buf.get_buffer());
  st->hash_computed = true;
  return true;
}

static bool computeSizes(ast_struct* st, SymbolTable* symtable) {
  if (st->csize > 0) return true;
  for (auto* elem : st->elements) {
    if (elem->array_suffix) {
      // if it is an array, in python this is a PyObject
      elem->csize = 8;
      elem->coffset = st->csize;
      st->csize += elem->csize;
      continue;
    }
    switch (elem->type) {
      case TYPE_BOOL:
      case TYPE_U8:
      case TYPE_S8:
        elem->csize = 1;
        break;
      case TYPE_U16:
      case TYPE_S16:
        elem->csize = 2;
        break;
      case TYPE_F32:
      case TYPE_U32:
      case TYPE_S32:
        elem->csize = 4;
        break;
      case TYPE_F64:
      case TYPE_U64:
      case TYPE_S64:
        elem->csize = 8;
        break;
      case TYPE_STRING:
      case TYPE_SHORT_STRING:
        // All of these are PyObject in python, need a pointer
        elem->csize = 8;
        break;
      case TYPE_CUSTOM: {
        if (symtable->find_enum(elem) != nullptr) {
          elem->csize = 4;
        } else {
          auto* inner_st = symtable->find_struct(elem);
          if (inner_st == nullptr) return false;
          bool bret = computeSizes(inner_st, symtable);
          if (!bret) return false;
          elem->csize = 8;
        }
      }
    }
    elem->coffset = st->csize;
    st->csize += elem->csize;
  }
  return true;
}

static int ElemTypeToPyCType(ast_element* e, SymbolTable* sym) {
  if (e->array_suffix) {
    return T_OBJECT;
  }
  switch (e->type) {
    case TYPE_BOOL:
      return T_BOOL;
    case TYPE_U8:
      return T_UBYTE;
    case TYPE_S8:
      return T_BYTE;
    case TYPE_U16:
      return T_USHORT;
    case TYPE_S16:
      return T_SHORT;
    case TYPE_F32:
      return T_FLOAT;
    case TYPE_U32:
      return T_UINT;
    case TYPE_S32:
      return T_INT;
    case TYPE_F64:
      return T_DOUBLE;
    case TYPE_U64:
      return T_ULONGLONG;
    case TYPE_S64:
      return T_LONGLONG;
    case TYPE_STRING:
    case TYPE_SHORT_STRING:
      return T_OBJECT;
    case TYPE_CUSTOM: {
      if (sym->find_enum(e) != nullptr) {
        return T_UINT;
      } else {
        auto* inner_st = sym->find_struct(e);
        if (inner_st == nullptr) {
          PyErr_Format(PyExc_TypeError, "Unknown custom type %s", e->custom_name);
          return 0;
        }
        return T_OBJECT;
      }
    }
  }
  PyErr_Format(PyExc_TypeError, "Unknown element type %d", e->type);
  return 0;
}

static PyObject* DynamicStr(PyObject* obj) {
  PyObject* mod = PyType_GetModule(obj->ob_type);
  PyCBuf_State* state = pycbufmodule_getstate(mod);
  if (state == nullptr) {
    PyErr_Format(PyExc_RuntimeError, "Cannot find module state");
    return nullptr;
  }

  pycbuf_preamble* pre = (pycbuf_preamble*)obj;
  if (!state->info_map->contains(pre->hash)) {
    PyErr_Format(PyExc_RuntimeError, "Cannot find hash %" PRIX64, pre->hash);
    return nullptr;
  }
  PyTypeInfo& pyinfo = (*state->info_map)[pre->hash];

  u8* byte_ptr = (u8*)obj;
  std::string retstr = "{";
  bool first = true;
  for (PyMemberDef* md = pyinfo.members; md->name != nullptr; md++) {
    if (!strcmp(md->name, "cbuf_members")) continue;
    if (!first) retstr += ", ";
    first = false;
    retstr += std::string{md->name} + ": ";
    switch (md->type) {
      case T_OBJECT: {
        PyObject* lobj = *(PyObject**)(byte_ptr + md->offset);
        if (lobj == nullptr) {
          retstr += "null";
        } else {
          retstr += PyUnicode_AsUTF8(PyObject_Str(lobj));
        }
        break;
      }
      case T_BOOL: {
        bool* b = (bool*)(byte_ptr + md->offset);
        retstr += (*b) ? "true" : "false";
        break;
      }
      case T_UBYTE: {
        retstr += std::to_string(*(u8*)(byte_ptr + md->offset));
        break;
      }
      case T_BYTE: {
        retstr += std::to_string(*(s8*)(byte_ptr + md->offset));
        break;
      }
      case T_USHORT: {
        retstr += std::to_string(*(u16*)(byte_ptr + md->offset));
        break;
      }
      case T_SHORT: {
        retstr += std::to_string(*(s16*)(byte_ptr + md->offset));
        break;
      }
      case T_FLOAT: {
        retstr += std::to_string(*(float*)(byte_ptr + md->offset));
        break;
      }
      case T_DOUBLE: {
        retstr += std::to_string(*(double*)(byte_ptr + md->offset));
        break;
      }
      case T_UINT: {
        retstr += std::to_string(*(u32*)(byte_ptr + md->offset));
        break;
      }
      case T_INT: {
        retstr += std::to_string(*(s32*)(byte_ptr + md->offset));
        break;
      }
      case T_ULONGLONG: {
        retstr += std::to_string(*(u64*)(byte_ptr + md->offset));
        break;
      }
      case T_LONGLONG: {
        retstr += std::to_string(*(s64*)(byte_ptr + md->offset));
        break;
      }
      default:
        PyErr_Format(PyExc_TypeError, "Unsupported type: %d", md->type);
        return nullptr;
    }
  }
  retstr += "}";

  return PyUnicode_FromStringAndSize(retstr.c_str(), retstr.size());
}

static void DynamicDealloc(PyObject* obj) {
  PyCBuf_State* state = (PyCBuf_State*)PyType_GetModuleState(Py_TYPE(obj));
  if (state == nullptr) {
    PyErr_Format(PyExc_RuntimeError, "Cannot find module state");
    return;
  }

  pycbuf_preamble* pre = (pycbuf_preamble*)obj;
  if (!state->info_map->contains(pre->hash)) {
    PyErr_Format(PyExc_RuntimeError, "Cannot find hash %" PRIX64, pre->hash);
    return;
  }
  PyTypeInfo& pyinfo = (*state->info_map)[pre->hash];

  u8* byte_ptr = (u8*)obj;
  for (PyMemberDef* md = pyinfo.members; md->name != nullptr; md++) {
    if (md->type == T_OBJECT) {
      Py_XDECREF(*(PyObject**)(byte_ptr + md->offset));
    }
  }
  Py_TYPE(obj)->tp_free(obj);
}

// This function defines a type, creates it if needed
PyTypeObject* CBufParserPy::GetPyTypeFromCBuf(uint64_t hash, ast_struct* st, PyObject* m,
                                              PyCBuf_State* state) {
  PyTypeObject* pyType = nullptr;
  if (state->info_map->contains(hash)) {
    pyType = (PyTypeObject*)(*state->info_map)[hash].type;
    if (pyType == nullptr) {
      PyErr_Format(PyExc_ValueError, "Missing PyTypeObject for hash %" PRIX64, hash);
      return nullptr;
    }
    if (PyType_Check(pyType) == 0) {
      PyErr_Format(PyExc_ValueError, "Invalid PyTypeObject for hash %" PRIX64, hash);
      return nullptr;
    }
    return pyType;
  }

  // We did not have a type, we have to create it
  if (state->info_map->contains(hash)) {
    PyErr_Format(PyExc_ValueError, "Hash collision %" PRIX64, hash);
    return nullptr;
  }
  PyTypeInfo& pyinfo = (*state->info_map)[hash];
  if (!computeSizes(st, sym)) {
    PyErr_Format(PyExc_ValueError, "Cannot compute sizes for hash %" PRIX64, hash);
    return nullptr;
  }
  //+1
  PyMemberDef* members = (PyMemberDef*)state->pool->alloc(sizeof(PyMemberDef) * (st->elements.size() + 2));
  for (int im = 0; im < st->elements.size(); im++) {
    members[im].name = st->elements[im]->name;
    members[im].doc = nullptr;
    members[im].flags = 0;
    members[im].offset = sizeof(pycbuf_preamble) + sizeof(PyObject*) + st->elements[im]->coffset;
    members[im].type = ElemTypeToPyCType(st->elements[im], sym);
  }
  members[st->elements.size()].name = "cbuf_members";
  members[st->elements.size()].doc = nullptr;
  members[st->elements.size()].flags = READONLY;
  members[st->elements.size()].offset = sizeof(pycbuf_preamble);
  members[st->elements.size()].type = T_OBJECT;

  //+1
  memset(&members[st->elements.size() + 1], 0, sizeof(PyMemberDef));
  pyinfo.members = members;

  PyType_Slot* slots = (PyType_Slot*)state->pool->alloc(sizeof(PyType_Slot) * 6);
  slots[0].slot = Py_tp_dealloc;
  slots[0].pfunc = (void*)DynamicDealloc;  // Dealloc function!
  slots[1].slot = Py_tp_base;
  slots[1].pfunc = state->PyCBufPreamble_Type;
  slots[2].slot = Py_tp_members;
  slots[2].pfunc = members;  // Members for all the cbufs
  slots[3].slot = Py_tp_str;
  slots[3].pfunc = (void*)DynamicStr;
  slots[4].slot = Py_tp_repr;
  slots[4].pfunc = (void*)DynamicStr;
  slots[5].slot = 0;
  slots[5].pfunc = nullptr;

  PyType_Spec* spec = (PyType_Spec*)state->pool->alloc(sizeof(PyType_Spec));
  int str_size = strlen(st->name) + 8 + 5;
  char* type_name = (char*)state->pool->alloc(str_size);
  snprintf(type_name, str_size, "pycbuf.%s_%" PRIX64, st->name, uint64_t((hash & 0x0FFFFULL)));
  spec->name = type_name;
  spec->itemsize = 0;
  spec->flags = Py_TPFLAGS_DEFAULT;
  spec->basicsize =
      sizeof(pycbuf_preamble) + st->csize + sizeof(PyObject*);  // Add here the needed space for this type!
  spec->slots = slots;
  pyinfo.spec = spec;
  pyinfo.parser = this;
  pyType = (PyTypeObject*)PyType_FromModuleAndSpec(m, spec, nullptr);
  if (pyType == nullptr) {
    state->info_map->erase(hash);
  } else {
    if (PyErr_Occurred()) return nullptr;

    pyinfo.type = (PyObject*)pyType;
    PyObject* list = PyList_New(st->elements.size());
    for (int i = 0; i < st->elements.size(); i++) {
      PyList_SetItem(list, i, PyUnicode_FromString(st->elements[i]->name));
    }
    Py_INCREF(list);
    pyinfo.list = list;

    // The +7 is to skip "pycbuf." prefix
    if (0 == PyModule_AddObject(m, type_name + 7, (PyObject*)pyType)) {
      // As per python requirements, only increment after success
      Py_INCREF(pyType);
    }
  }
  return pyType;
}

bool CBufParserPy::FillPyObjectInternal(uint64_t hash, ast_struct* st, PyObject* m, PyObject*& obj,
                                        PyCBuf_State* state) {
  if (!compute_hash(st, sym)) {
    PyErr_Format(PyExc_ValueError, "Cannot compute hash for type %s (hash %" PRIX64 ")", st->name, hash);
    obj = nullptr;
    success = false;
    return false;
  }
  // We cannot ensure we have a hash, like for naked structs. Add this code to ensure things and then drop the
  // hash
  if (hash > 0) {
    if (st->hash_value != hash) {
      PyErr_Format(PyExc_ValueError, "Hash mismatch decoding type `%s`, expected %" PRIX64 ", got %" PRIX64,
                   st->name, st->hash_value, hash);
      obj = nullptr;
      success = false;
      return false;
    }
  } else {
    hash = st->hash_value;
  }

  // Get the python type for this hash and ast_struct
  PyTypeObject* pyType = GetPyTypeFromCBuf(hash, st, m, state);
  if (pyType == nullptr) {
    PyErr_Format(PyExc_ValueError, "Failed to create PyTypeObject for hash %" PRIX64, hash);
    obj = nullptr;
    success = false;
    return false;
  }

  // PyErr_Occurred()

  // Create a python object of that type (EXTRA: use default values from cbuf)
  obj = PyObject_CallObject((PyObject*)pyType, nullptr);
  if (obj == nullptr) {
    // This will allow python to create exceptions
    success = false;
    return false;
  }

  // Fill the object members
  // All structs have a preamble, skip it
  if (!st->naked) {
    const cbuf_preamble* pre = (const cbuf_preamble*)buffer;
    pycbuf_preamble* pypre = (pycbuf_preamble*)obj;
    pypre->hash = pre->hash;
    pypre->packet_timest = pre->packet_timest;
    pypre->magic = pre->magic;
    pypre->size_ = pre->size();
    pypre->variant = pre->variant();
    pypre->type_name = st->name;
    pypre->source_name = source_cbuf_file_;
    if (pre->hash != hash) {
      PyErr_Format(PyExc_ValueError, "Hash mismatch decoding type `%s`, expected %" PRIX64 ", got %" PRIX64,
                   st->name, hash, pre->hash);
      success = false;
      return false;
    }
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  } else {
    pycbuf_preamble* pypre = (pycbuf_preamble*)obj;
    pypre->hash = hash;
    pypre->type_name = st->name;
    pypre->source_name = source_cbuf_file_;
    pypre->packet_timest = current_timestamp_;
    pypre->magic = magic_;
  }

  if (!state->info_map->contains(((pycbuf_preamble*)obj)->hash)) {
    success = false;
    return false;
  }

  PyTypeInfo& pyInfo = (*state->info_map)[hash];

  // Once done, assign it
  u8* byte_ptr = (u8*)obj;
  *(PyObject**)(byte_ptr + sizeof(pycbuf_preamble)) = pyInfo.list;
  Py_INCREF(pyInfo.list);

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    // Uncomment this assert to catch things while debugging
    // without it, python will issue an exception users can see
    // assert(PyErr_Occurred() == nullptr);
    auto& elem = st->elements[elem_idx];
    if (!success) return false;
    switch (elem->type) {
      case TYPE_U8: {
        success = process_element_py<u8>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_U16: {
        success = process_element_py<u16>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_U32: {
        success = process_element_py<u32>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_U64: {
        success = process_element_py<u64>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_S8: {
        success = process_element_py<s8>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_S16: {
        success = process_element_py<s16>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_S32: {
        success = process_element_py<s32>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_S64: {
        success = process_element_py<s64>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_F32: {
        success = process_element_py<f32>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_F64: {
        success = process_element_py<f64>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_BOOL: {
        success = process_element_py<u8>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_STRING: {
        success = process_element_string_py(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_SHORT_STRING: {
        success = process_element_short_string_py(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
        break;
      }
      case TYPE_CUSTOM: {
        u8* byte_ptr = (u8*)obj;
        if (elem->array_suffix) {
          if (sym->find_enum(elem)) {
            // If this is an array of enum, the function process_element_py will handle the array
            // We need to call this here so the function can consume the array size
            process_element_py<u32>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
            continue;
          }

          // This is an array
          u32 array_size;
          if (elem->is_dynamic_array || elem->is_compact_array) {
            // This is a dynamic array
            array_size = *(const u32*)buffer;
            buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
          } else {
            // this is a static array
            array_size = elem->array_suffix->size;
          }
          if (elem->is_compact_array && array_size > elem->array_suffix->size) {
            success = false;
            return false;
          }

          // Create Python Array/List

          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            PyObject* list = PyList_New(array_size);
            for (u32 i = 0; i < array_size; i++) {
              PyObject* subobj = nullptr;
              FillPyObjectInternal(0, inst, m, subobj, state);
              if (!success) return false;
              PyList_SET_ITEM(list, i, subobj);
            }
            *(PyObject**)(byte_ptr + pyInfo.members[elem_idx].offset) = list;
          } else {
            // We handled this case above, no need to do so here
            fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
            return false;
          }

        } else {
          // This is a single element, not an array
          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            PyObject* subobj = nullptr;
            FillPyObjectInternal(0, inst, m, subobj, state);
            *(PyObject**)(byte_ptr + pyInfo.members[elem_idx].offset) = subobj;
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            // this is right because the enum is directly on the obj struct
            process_element_py<u32>(elem, buffer, buf_size, m, obj, &pyInfo.members[elem_idx]);
          }
        }

        break;
      }
    }
  }
  return success;
}

unsigned int CBufParserPy::FillPyObject(uint64_t hash, const char* st_name, const unsigned char* bin_buffer,
                                        size_t bin_buf_size, const char* source_cbuf_file, PyObject* m,
                                        PyObject*& obj) {
  buffer = bin_buffer;
  buf_size = bin_buf_size;
  source_cbuf_file_ = nullptr;
  success = true;

  ast_struct* cbuf_type = decompose_and_find(st_name);
  if (cbuf_type == nullptr) {
    obj = nullptr;
    buffer = nullptr;
    return 0;
  }

  const cbuf_preamble* pre = (const cbuf_preamble*)buffer;
  if (pre->hash != hash) {
    PyErr_Format(PyExc_ValueError, "Hash mismatch decoding type `%s`, expected %" PRIX64 ", got %" PRIX64,
                 cbuf_type->name, hash, pre->hash);
    obj = nullptr;
    buffer = nullptr;
    return 0;
  }
  current_timestamp_ = pre->packet_timest;
  magic_ = pre->magic;

  PyCBuf_State* state = pycbufmodule_getstate(m);
  for (auto f : *(state->info_sources)) {
    if (!strcmp(f, source_cbuf_file)) {
      source_cbuf_file_ = f;
      break;
    }
  }
  if (source_cbuf_file_ == nullptr) {
    source_cbuf_file_ = strdup(source_cbuf_file);
    state->info_sources->push_back(source_cbuf_file_);
  }

  if (!FillPyObjectInternal(hash, cbuf_type, m, obj, state)) {
    obj = nullptr;
    buffer = nullptr;
    source_cbuf_file_ = nullptr;
    return 0;
  }

  buffer = nullptr;
  source_cbuf_file_ = nullptr;
  return bin_buf_size - buf_size;
}

CBufParserPy::CBufParserPy() {}
CBufParserPy::~CBufParserPy() {}
