#pragma once
#include <hjson.h>
#include <toolbox/hjson_helper.h> // this header has json helpers
// Please include all the required cbuf C headers before this file
#include "metadata.h"
template<typename T>
void loadFromJson(const Hjson::Value& json, T&obj);


#if !defined(_JSON_DECLARATION_metadata_)
#define _JSON_DECLARATION_metadata_
template<>
  void loadFromJson(const Hjson::Value& json, cbufmsg::metadata& obj);
#endif // _JSON_DECLARATION_metadata_



#if !defined(_JSON_IMPLEMENTATION_metadata_)
#define _JSON_IMPLEMENTATION_metadata_
template<>
inline void loadFromJson(const Hjson::Value& json, cbufmsg::metadata& obj)
{
    do { // Loading msg_hash
        get_member_uint(json, "msg_hash", obj.msg_hash);
    } while(0);
    do { // Loading msg_name
        get_member_string(json, "msg_name", obj.msg_name);
    } while(0);
    do { // Loading msg_meta
        get_member_string(json, "msg_meta", obj.msg_meta);
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_metadata_



