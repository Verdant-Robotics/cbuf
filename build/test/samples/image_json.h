#pragma once
#include <hjson.h>
#include <toolbox/hjson_helper.h> // this header has json helpers
// Please include all the required cbuf C headers before this file
#include "image.h"
template<typename T>
void loadFromJson(const Hjson::Value& json, T&obj);

#include "inctype_json.h"

#if !defined(_JSON_DECLARATION_silly1_)
#define _JSON_DECLARATION_silly1_
template<>
  void loadFromJson(const Hjson::Value& json, outer::silly1& obj);
#endif // _JSON_DECLARATION_silly1_
#if !defined(_JSON_DECLARATION_inctype_)
#define _JSON_DECLARATION_inctype_
template<>
  void loadFromJson(const Hjson::Value& json, messages::inctype& obj);
#endif // _JSON_DECLARATION_inctype_
#if !defined(_JSON_DECLARATION_naked_struct_)
#define _JSON_DECLARATION_naked_struct_
template<>
  void loadFromJson(const Hjson::Value& json, messages::naked_struct& obj);
#endif // _JSON_DECLARATION_naked_struct_
#if !defined(_JSON_DECLARATION_image_)
#define _JSON_DECLARATION_image_
template<>
  void loadFromJson(const Hjson::Value& json, messages::image& obj);
#endif // _JSON_DECLARATION_image_
#if !defined(_JSON_DECLARATION_complex_thing_)
#define _JSON_DECLARATION_complex_thing_
template<>
  void loadFromJson(const Hjson::Value& json, messages::complex_thing& obj);
#endif // _JSON_DECLARATION_complex_thing_
#if !defined(_JSON_DECLARATION_fourmegs_)
#define _JSON_DECLARATION_fourmegs_
template<>
  void loadFromJson(const Hjson::Value& json, messages::fourmegs& obj);
#endif // _JSON_DECLARATION_fourmegs_
#if !defined(_JSON_DECLARATION_getslarge_)
#define _JSON_DECLARATION_getslarge_
template<>
  void loadFromJson(const Hjson::Value& json, messages::getslarge& obj);
#endif // _JSON_DECLARATION_getslarge_



#if !defined(_JSON_IMPLEMENTATION_silly1_)
#define _JSON_IMPLEMENTATION_silly1_
template<>
inline void loadFromJson(const Hjson::Value& json, outer::silly1& obj)
{
    do { // Loading val1
        get_member_uint(json, "val1", obj.val1);
    } while(0);
    do { // Loading val2
        get_member_uint(json, "val2", obj.val2);
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_silly1_



#if !defined(_JSON_IMPLEMENTATION_inctype_)
#define _JSON_IMPLEMENTATION_inctype_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::inctype& obj)
{
    do { // Loading sensor_name
        {
            std::string tmp;
            if (get_member_string(json, "sensor_name", tmp)) {
                obj.sensor_name = tmp;
            }
        }
    } while(0);
    do { // Loading val
        get_member_uint(json, "val", obj.val);
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_inctype_

#if !defined(_JSON_IMPLEMENTATION_naked_struct_)
#define _JSON_IMPLEMENTATION_naked_struct_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::naked_struct& obj)
{
    do { // Loading value1
        get_member_uint(json, "value1", obj.value1);
    } while(0);
    do { // Loading value2
        get_member_uint(json, "value2", obj.value2);
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_naked_struct_

#if !defined(_JSON_IMPLEMENTATION_image_)
#define _JSON_IMPLEMENTATION_image_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::image& obj)
{
    do { // Loading sensor_name
        {
            std::string tmp;
            if (get_member_string(json, "sensor_name", tmp)) {
                obj.sensor_name = tmp;
            }
        }
    } while(0);
    do { // Loading rows
        get_member_uint(json, "rows", obj.rows);
    } while(0);
    do { // Loading cols
        get_member_uint(json, "cols", obj.cols);
    } while(0);
    do { // Loading format
        get_member_int(json, "format", obj.format);
    } while(0);
    do { // Loading type
        get_member_int(json, "type", obj.type);
    } while(0);
    do { // Loading timestamp
        get_member_double(json, "timestamp", obj.timestamp);
    } while(0);
    do { // Loading pixels
        if (!json["pixels"].defined()) break;
        uint32_t num_pixels = 3072;
        for( int pixels_index=0; pixels_index < num_pixels; pixels_index++) {
            const Hjson::Value& jelem = json["pixels"][pixels_index];
            auto& elem = obj.pixels[pixels_index];
            get_value_uint(jelem, elem);
        }
    } while(0);
    do { // Loading neginit
        get_member_double(json, "neginit", obj.neginit);
    } while(0);
    do { // Loading dexpinit
        get_member_double(json, "dexpinit", obj.dexpinit);
    } while(0);
    do { // Loading iexpinit
        get_member_int(json, "iexpinit", obj.iexpinit);
    } while(0);
    do { // Loading fancy
        {
            int fancy_int;
            if (get_member_int(json, "fancy", fancy_int)) {
                obj.fancy = messages::MyFancyEnum(fancy_int);
            }
        }
    } while(0);
    do { // Loading sill
        loadFromJson(json["sill"], obj.sill);
    } while(0);
    do { // Loading includes
        loadFromJson(json["includes"], obj.includes);
    } while(0);
    do { // Loading nstruct
        loadFromJson(json["nstruct"], obj.nstruct);
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_image_

#if !defined(_JSON_IMPLEMENTATION_complex_thing_)
#define _JSON_IMPLEMENTATION_complex_thing_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::complex_thing& obj)
{
    do { // Loading one_val
        get_member_int(json, "one_val", obj.one_val);
    } while(0);
    do { // Loading fancy2
        {
            int fancy2_int;
            if (get_member_int(json, "fancy2", fancy2_int)) {
                obj.fancy2 = messages::MyFancyEnum(fancy2_int);
            }
        }
    } while(0);
    do { // Loading dynamic_array
        const Hjson::Value& vec_dynamic_array = json["dynamic_array"];
        if (!vec_dynamic_array.defined()) break;
        obj.dynamic_array.resize(vec_dynamic_array.size());
        for( int dynamic_array_index=0; dynamic_array_index < vec_dynamic_array.size(); dynamic_array_index++) {
            const Hjson::Value& jelem = json["dynamic_array"][dynamic_array_index];
            auto& elem = obj.dynamic_array[dynamic_array_index];
            get_value_int(jelem, elem);
        }
    } while(0);
    do { // Loading compact_array
        if (!json["compact_array"].defined()) break;
        obj.num_compact_array = json["compact_array"].size();
        for( int compact_array_index=0; compact_array_index < obj.num_compact_array; compact_array_index++) {
            const Hjson::Value& jelem = json["compact_array"][compact_array_index];
            auto& elem = obj.compact_array[compact_array_index];
            get_value_int(jelem, elem);
        }
    } while(0);
    do { // Loading name
        get_member_string(json, "name", obj.name);
    } while(0);
    do { // Loading img
        if (!json["img"].defined()) break;
        uint32_t num_img = 3;
        for( int img_index=0; img_index < num_img; img_index++) {
            const Hjson::Value& jelem = json["img"][img_index];
            auto& elem = obj.img[img_index];
            loadFromJson(jelem, elem);
        }
    } while(0);
    do { // Loading sill
        loadFromJson(json["sill"], obj.sill);
    } while(0);
    do { // Loading names
        if (!json["names"].defined()) break;
        uint32_t num_names = 5;
        for( int names_index=0; names_index < num_names; names_index++) {
            const Hjson::Value& jelem = json["names"][names_index];
            auto& elem = obj.names[names_index];
            get_value_string(jelem, elem);
        }
    } while(0);
    do { // Loading hard_dynamic
        const Hjson::Value& vec_hard_dynamic = json["hard_dynamic"];
        if (!vec_hard_dynamic.defined()) break;
        obj.hard_dynamic.resize(vec_hard_dynamic.size());
        for( int hard_dynamic_index=0; hard_dynamic_index < vec_hard_dynamic.size(); hard_dynamic_index++) {
            const Hjson::Value& jelem = json["hard_dynamic"][hard_dynamic_index];
            auto& elem = obj.hard_dynamic[hard_dynamic_index];
            get_value_string(jelem, elem);
        }
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_complex_thing_

#if !defined(_JSON_IMPLEMENTATION_fourmegs_)
#define _JSON_IMPLEMENTATION_fourmegs_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::fourmegs& obj)
{
    do { // Loading data
        if (!json["data"].defined()) break;
        uint32_t num_data = 1048576;
        for( int data_index=0; data_index < num_data; data_index++) {
            const Hjson::Value& jelem = json["data"][data_index];
            auto& elem = obj.data[data_index];
            get_value_uint(jelem, elem);
        }
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_fourmegs_

#if !defined(_JSON_IMPLEMENTATION_getslarge_)
#define _JSON_IMPLEMENTATION_getslarge_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::getslarge& obj)
{
    do { // Loading vec
        const Hjson::Value& vec_vec = json["vec"];
        if (!vec_vec.defined()) break;
        obj.vec.resize(vec_vec.size());
        for( int vec_index=0; vec_index < vec_vec.size(); vec_index++) {
            const Hjson::Value& jelem = json["vec"][vec_index];
            auto& elem = obj.vec[vec_index];
            loadFromJson(jelem, elem);
        }
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_getslarge_



