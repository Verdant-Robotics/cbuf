#pragma once
#include "cbuf_preamble.h"
#include <stdint.h> // uint8_t and such
#include <string.h> // memcpy
#include <vector>   // std::vector
#include <string>   // std::string
#include "vstring.h"

#include "inctype.h"


namespace outer {
    
    struct ATTR_PACKED silly1 {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            sizeof(silly1),
            0x14ABA7E653BCA7F8,
            0.0,
            };
        uint32_t val1;
        uint32_t val2;
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0x14ABA7E653BCA7F8;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "outer::silly1";
        static bool is_simple() { return true; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            val1= 0;
            val2= 0;
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
        }

        size_t encode_size() const
        {
            return sizeof(silly1);
        }

        void free_encode(const char *) const {}

        bool encode(char *data, unsigned int buf_size) const
        {
            if (buf_size < sizeof(silly1)) return false;
            memcpy(data, this, sizeof(*this));
            return true;
        }

        // This variant allows for no copy when writing to disk.
        const char *encode() const
        {
            return reinterpret_cast<const char *>(this);
        }

        bool decode(char *data, unsigned int buf_size)
        {
            if (buf_size < sizeof(silly1)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            memcpy(this, data, sizeof(*this));
            return true;
        }

        static bool decode(char *data, unsigned int buf_size, silly1** var)
        {
            if (buf_size < sizeof(silly1)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            *var = reinterpret_cast<silly1 *>(data);
            return true;
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += sizeof(uint32_t); // for val1
            ret_size += sizeof(uint32_t); // for val2
            return ret_size;
        }

        bool encode_net(char *data, unsigned int buf_size) const
        {
            preamble.setSize(uint32_t(encode_net_size()));
            if (buf_size < preamble.size()) return false;
            *reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = preamble.size_;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint64_t *>(data) = preamble.hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<double *>(data) = preamble.packet_timest;
            data += sizeof(double);
            *reinterpret_cast<uint32_t *>(data) = this->val1;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = this->val2;
            data += sizeof(uint32_t);
            
            return true;
        }

        bool decode_net(char *data, unsigned int buf_size)
        {
            uint32_t magic = *reinterpret_cast<uint32_t *>(data);
            if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();
            preamble.magic = magic;
            data += sizeof(uint32_t);
            uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);
            preamble.size_ = dec_size;
            if (preamble.size() > buf_size) CBUF_RETURN_FALSE();
            data += sizeof(uint32_t);
            uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);
            if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();
            data += sizeof(uint64_t);
            preamble.packet_timest = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            this->val1 = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->val2 = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace outer {
    struct silly1 {
        uint32_t val1;
        uint32_t val2;
    }
}

)CBUF_CODE";

    };

};

namespace messages {
    constexpr const char * kStrName = "mystring";
    constexpr uint64_t myflag = 0xFFFFFFFF;
    
    enum MyFancyEnum
    {
        VALUE_1 = -3,
        VALUE_2,
        VALUE_3 = 32,
    };

    struct ATTR_PACKED naked_struct {
        // There is no preamble, this is a naked struct

        uint32_t value1;
        uint32_t value2;
        static bool is_simple() { return true; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            value1= 0;
            value2= 0;
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
        }

        size_t encode_size() const
        {
            return sizeof(naked_struct);
        }

        void free_encode(const char *) const {}

        bool encode_naked(char *data, unsigned int buf_size) const
        {
            if (buf_size < sizeof(naked_struct)) return false;
            memcpy(data, this, sizeof(*this));
            return true;
        }

        // This variant allows for no copy when writing to disk.
        const char *encode_naked() const
        {
            return reinterpret_cast<const char *>(this);
        }

        bool decode_naked(char *data, unsigned int buf_size)
        {
            if (buf_size < sizeof(naked_struct)) return false;
            memcpy(this, data, sizeof(*this));
            return true;
        }

        static bool decode_naked(char *data, unsigned int buf_size, naked_struct** var)
        {
            if (buf_size < sizeof(naked_struct)) return false;
            *var = reinterpret_cast<naked_struct *>(data);
            return true;
        }

        size_t encode_net_size() const
        {
            size_t ret_size = 0;
            ret_size += sizeof(uint32_t); // for value1
            ret_size += sizeof(uint32_t); // for value2
            return ret_size;
        }

        bool encode_net_naked(char *data, unsigned int buf_size) const
        {
            *reinterpret_cast<uint32_t *>(data) = this->value1;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = this->value2;
            data += sizeof(uint32_t);
            
            return true;
        }

        bool decode_net_naked(char *data, unsigned int buf_size)
        {
            this->value1 = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->value2 = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace outer {
    struct silly1 {
        uint32_t val1;
        uint32_t val2;
    }
}

namespace messages {
    enum MyFancyEnum{
        VALUE_1 = -3,
        VALUE_2,
        VALUE_3 = 32,
    }
    struct inctype {
        short_string sensor_name = "somename";
        uint32_t val = 10;
    }
    struct naked_struct @naked {
        uint32_t value1;
        uint32_t value2;
    }
    struct image {
        short_string sensor_name;
        uint32_t rows;
        uint32_t cols = 3;
        int32_t format;
        int32_t type;
        double timestamp = 1.300000;
        uint8_t pixels[3072];
        double neginit = -1.300000;
        double dexpinit = 2.518519;
        int32_t iexpinit = 2076;
        MyFancyEnum fancy;
        outer::silly1 sill;
        inctype includes;
        naked_struct nstruct;
    }
    struct complex_thing {
        int32_t one_val;
        MyFancyEnum fancy2;
        int32_t dynamic_array[];
        int32_t compact_array[100] @compact;
        string name;
        image img[3];
        outer::silly1 sill;
        string names[5];
        string hard_dynamic[];
    }
    struct fourmegs {
        uint32_t data[1048576];
    }
    struct getslarge {
        fourmegs vec[];
    }
}

)CBUF_CODE";

    };

    struct ATTR_PACKED image {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            sizeof(image),
            0xEE4410751C8A337,
            0.0,
            };
        VString<15> sensor_name;
        uint32_t rows;
        uint32_t cols = 3;
        int32_t format;
        int32_t type;
        double timestamp = 1.300000;
        uint8_t pixels[3072];
        double neginit = -1.300000;
        double dexpinit = 2.518519;
        int32_t iexpinit = 2076;
        MyFancyEnum fancy = VALUE_2;
        outer::silly1 sill;
        inctype includes;
        naked_struct nstruct;
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0xEE4410751C8A337;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "messages::image";
        static bool is_simple() { return true; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            sensor_name= "";
            rows= 0;
            cols = 3;
            format= 0;
            type= 0;
            timestamp = 1.300000;
            for(int pixels_index = 0; pixels_index < 3072; pixels_index++) {
                pixels[pixels_index] = 0;
            }
            neginit = -1.300000;
            dexpinit = 2.518519;
            iexpinit = 2076;
            fancy = VALUE_2;
            sill.Init();
            includes.Init();
            nstruct.Init();
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
            outer::silly1::handle_metadata(fn, ctx);
            inctype::handle_metadata(fn, ctx);
            naked_struct::handle_metadata(fn, ctx);
        }

        size_t encode_size() const
        {
            return sizeof(image);
        }

        void free_encode(const char *) const {}

        bool encode(char *data, unsigned int buf_size) const
        {
            if (buf_size < sizeof(image)) return false;
            memcpy(data, this, sizeof(*this));
            return true;
        }

        // This variant allows for no copy when writing to disk.
        const char *encode() const
        {
            return reinterpret_cast<const char *>(this);
        }

        bool decode(char *data, unsigned int buf_size)
        {
            if (buf_size < sizeof(image)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            memcpy(this, data, sizeof(*this));
            return true;
        }

        static bool decode(char *data, unsigned int buf_size, image** var)
        {
            if (buf_size < sizeof(image)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            *var = reinterpret_cast<image *>(data);
            return true;
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += sizeof(VString<15>); // for sensor_name
            ret_size += sizeof(uint32_t); // for rows
            ret_size += sizeof(uint32_t); // for cols
            ret_size += sizeof(int32_t); // for format
            ret_size += sizeof(int32_t); // for type
            ret_size += sizeof(double); // for timestamp
            ret_size += sizeof(uint8_t) * 3072;
            ret_size += sizeof(double); // for neginit
            ret_size += sizeof(double); // for dexpinit
            ret_size += sizeof(int32_t); // for iexpinit
            ret_size += sizeof(int32_t);
            ret_size += sill.encode_net_size();
            ret_size += includes.encode_net_size();
            ret_size += nstruct.encode_net_size();
            return ret_size;
        }

        bool encode_net(char *data, unsigned int buf_size) const
        {
            preamble.setSize(uint32_t(encode_net_size()));
            if (buf_size < preamble.size()) return false;
            *reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = preamble.size_;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint64_t *>(data) = preamble.hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<double *>(data) = preamble.packet_timest;
            data += sizeof(double);
            *reinterpret_cast<VString<15> *>(data) = this->sensor_name;
            data += sizeof(VString<15>);
            *reinterpret_cast<uint32_t *>(data) = this->rows;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = this->cols;
            data += sizeof(uint32_t);
            *reinterpret_cast<int32_t *>(data) = this->format;
            data += sizeof(int32_t);
            *reinterpret_cast<int32_t *>(data) = this->type;
            data += sizeof(int32_t);
            *reinterpret_cast<double *>(data) = this->timestamp;
            data += sizeof(double);
            memcpy(data, this->pixels, 3072*sizeof(uint8_t));
            data += 3072*sizeof(uint8_t);
            *reinterpret_cast<double *>(data) = this->neginit;
            data += sizeof(double);
            *reinterpret_cast<double *>(data) = this->dexpinit;
            data += sizeof(double);
            *reinterpret_cast<int32_t *>(data) = this->iexpinit;
            data += sizeof(int32_t);
            *reinterpret_cast<int32_t *>(data) = int32_t(this->fancy);
            data += sizeof(int32_t);
            sill.encode_net(data, buf_size);
            data += sill.encode_net_size();
            includes.encode_net(data, buf_size);
            data += includes.encode_net_size();
            nstruct.encode_net_naked(data, buf_size);
            data += nstruct.encode_net_size();
            
            return true;
        }

        bool decode_net(char *data, unsigned int buf_size)
        {
            uint32_t magic = *reinterpret_cast<uint32_t *>(data);
            if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();
            preamble.magic = magic;
            data += sizeof(uint32_t);
            uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);
            preamble.size_ = dec_size;
            if (preamble.size() > buf_size) CBUF_RETURN_FALSE();
            data += sizeof(uint32_t);
            uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);
            if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();
            data += sizeof(uint64_t);
            preamble.packet_timest = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            this->sensor_name = *reinterpret_cast<VString<15> *>(data);
            data += sizeof(VString<15>);
            this->rows = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->cols = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->format = *reinterpret_cast<int32_t *>(data);
            data += sizeof(int32_t);
            this->type = *reinterpret_cast<int32_t *>(data);
            data += sizeof(int32_t);
            this->timestamp = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            memcpy(this->pixels, data, 3072*sizeof(uint8_t));
            data += 3072*sizeof(uint8_t);
            this->neginit = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            this->dexpinit = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            this->iexpinit = *reinterpret_cast<int32_t *>(data);
            data += sizeof(int32_t);
            this->fancy = *reinterpret_cast<MyFancyEnum *>(data);
            data += sizeof(int32_t);
            if (!this->sill.decode_net(data, buf_size)) CBUF_RETURN_FALSE();
            data += this->sill.preamble.size();
            if (!this->includes.decode_net(data, buf_size)) CBUF_RETURN_FALSE();
            data += this->includes.preamble.size();
            if (!this->nstruct.decode_net_naked(data, buf_size)) CBUF_RETURN_FALSE();
            data += this->nstruct.encode_net_size();
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace outer {
    struct silly1 {
        uint32_t val1;
        uint32_t val2;
    }
}

namespace messages {
    enum MyFancyEnum{
        VALUE_1 = -3,
        VALUE_2,
        VALUE_3 = 32,
    }
    struct inctype {
        short_string sensor_name = "somename";
        uint32_t val = 10;
    }
    struct naked_struct @naked {
        uint32_t value1;
        uint32_t value2;
    }
    struct image {
        short_string sensor_name;
        uint32_t rows;
        uint32_t cols = 3;
        int32_t format;
        int32_t type;
        double timestamp = 1.300000;
        uint8_t pixels[3072];
        double neginit = -1.300000;
        double dexpinit = 2.518519;
        int32_t iexpinit = 2076;
        MyFancyEnum fancy;
        outer::silly1 sill;
        inctype includes;
        naked_struct nstruct;
    }
    struct complex_thing {
        int32_t one_val;
        MyFancyEnum fancy2;
        int32_t dynamic_array[];
        int32_t compact_array[100] @compact;
        string name;
        image img[3];
        outer::silly1 sill;
        string names[5];
        string hard_dynamic[];
    }
    struct fourmegs {
        uint32_t data[1048576];
    }
    struct getslarge {
        fourmegs vec[];
    }
}

)CBUF_CODE";

    };

    struct ATTR_PACKED complex_thing {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            0,
            0x70AF52E14953645,
            0.0,
            };
        int32_t one_val;
        MyFancyEnum fancy2;
        std::vector< int32_t > dynamic_array;
        uint32_t num_compact_array = 0;
        int32_t compact_array[100];
        std::string name;
        image img[3];
        outer::silly1 sill;
        std::string names[5];
        std::vector< std::string > hard_dynamic;
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0x70AF52E14953645;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "messages::complex_thing";
        static bool is_simple() { return false; }
        static bool supports_compact() { return true; }
        
        void Init()
        {
            one_val= 0;
            fancy2 = MyFancyEnum(0);
            dynamic_array.clear();
            num_compact_array = 0;
            for(int compact_array_index = 0; compact_array_index < 100; compact_array_index++) {
                compact_array[compact_array_index] = 0;
            }
            name= "";
            for(int img_index = 0; img_index < 3; img_index++) {
                img[img_index].Init();
            }
            sill.Init();
            for(int names_index = 0; names_index < 5; names_index++) {
                names[names_index] = "";
            }
            hard_dynamic.clear();
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
            image::handle_metadata(fn, ctx);
            outer::silly1::handle_metadata(fn, ctx);
        }

        size_t encode_size() const
        {
            return encode_net_size();
        }

        void free_encode(char *p) const
        {
            free(p);
        }

        bool encode(char *data, unsigned int buf_size) const
        {
            return encode_net(data, buf_size);
        }

        char *encode() const
        {
            size_t __struct_size = encode_size();
            preamble.setSize(uint32_t(__struct_size));
            char *data = reinterpret_cast<char *>(malloc(__struct_size));
            encode(data, __struct_size);
            return data;
        }

        bool decode(char *data, unsigned int buf_size)
        {
            return decode_net(data, buf_size);
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += sizeof(int32_t); // for one_val
            ret_size += sizeof(int32_t);
            ret_size += sizeof(int32_t) * dynamic_array.size();
            ret_size += sizeof(uint32_t); // Encode the length of dynamic_array
            ret_size += sizeof(int32_t) * num_compact_array;
            ret_size += sizeof(uint32_t); // Encode the length of compact_array in the var num_compact_array
            ret_size += name.length() + 4;
            ret_size += img[0].encode_net_size() * 3;
            ret_size += sill.encode_net_size();
            for (auto &names_elem: names) {
                ret_size += names_elem.length() + 4;
            }
            ret_size += sizeof(uint32_t); // To account for the length of hard_dynamic
            for (auto &hard_dynamic_elem: hard_dynamic) {
                ret_size += hard_dynamic_elem.length() + 4;
            }
            return ret_size;
        }

        bool encode_net(char *data, unsigned int buf_size) const
        {
            preamble.setSize(uint32_t(encode_net_size()));
            if (buf_size < preamble.size()) return false;
            *reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = preamble.size_;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint64_t *>(data) = preamble.hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<double *>(data) = preamble.packet_timest;
            data += sizeof(double);
            *reinterpret_cast<int32_t *>(data) = this->one_val;
            data += sizeof(int32_t);
            *reinterpret_cast<int32_t *>(data) = int32_t(this->fancy2);
            data += sizeof(int32_t);
            *reinterpret_cast<uint32_t *>(data) = uint32_t(this->dynamic_array.size());
            data += sizeof(uint32_t);
            memcpy(data, &this->dynamic_array[0], this->dynamic_array.size()*sizeof(int32_t));
            data += this->dynamic_array.size()*sizeof(int32_t);
            *reinterpret_cast<uint32_t *>(data) = num_compact_array;
            data += sizeof(uint32_t);
            memcpy(data, &this->compact_array[0], num_compact_array*sizeof(int32_t));
            data += num_compact_array*sizeof(int32_t);
            *reinterpret_cast<uint32_t *>(data) = uint32_t(this->name.length());
            data += sizeof(uint32_t);
            memcpy(data, this->name.c_str(), this->name.length());
            data += this->name.length();
            memcpy(data, this->img, 3*this->img[0].encode_net_size());
            data += 3*this->img[0].encode_net_size();
            sill.encode_net(data, buf_size);
            data += sill.encode_net_size();
            for(auto &names_elem: names) {
                *reinterpret_cast<uint32_t *>(data) = uint32_t(names_elem.length());
                data += sizeof(uint32_t);
                memcpy(data, names_elem.c_str(), names_elem.length());
                data += names_elem.length();
            }
            *reinterpret_cast<uint32_t *>(data) = uint32_t(this->hard_dynamic.size());
            data += sizeof(uint32_t);
            for(auto &hard_dynamic_elem: hard_dynamic) {
                *reinterpret_cast<uint32_t *>(data) = uint32_t(hard_dynamic_elem.length());
                data += sizeof(uint32_t);
                memcpy(data, hard_dynamic_elem.c_str(), hard_dynamic_elem.length());
                data += hard_dynamic_elem.length();
            }
            
            return true;
        }

        bool decode_net(char *data, unsigned int buf_size)
        {
            uint32_t magic = *reinterpret_cast<uint32_t *>(data);
            if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();
            preamble.magic = magic;
            data += sizeof(uint32_t);
            uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);
            preamble.size_ = dec_size;
            if (preamble.size() > buf_size) CBUF_RETURN_FALSE();
            data += sizeof(uint32_t);
            uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);
            if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();
            data += sizeof(uint64_t);
            preamble.packet_timest = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            this->one_val = *reinterpret_cast<int32_t *>(data);
            data += sizeof(int32_t);
            this->fancy2 = *reinterpret_cast<MyFancyEnum *>(data);
            data += sizeof(int32_t);
            uint32_t dynamic_array_count = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->dynamic_array.resize(dynamic_array_count);
            memcpy(&this->dynamic_array[0], data, dynamic_array_count*sizeof(int32_t));
            data += dynamic_array_count*sizeof(int32_t);
            num_compact_array = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            if (num_compact_array > sizeof(this->compact_array)/sizeof(int32_t)) CBUF_RETURN_FALSE();
            memcpy(this->compact_array, data, num_compact_array*sizeof(int32_t));
            data += num_compact_array*sizeof(int32_t);
            uint32_t name_length = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->name.assign(data, name_length);
            data += name_length;
            memcpy(this->img, data, 3*this->img[0].encode_net_size());
            data += 3*this->img[0].encode_net_size();
            if (!this->sill.decode_net(data, buf_size)) CBUF_RETURN_FALSE();
            data += this->sill.preamble.size();
            for(uint32_t i=0; i<5; i++) {
                uint32_t names_length = *reinterpret_cast<uint32_t *>(data);
                data += sizeof(uint32_t);
                this->names[i].assign(data, names_length);
                data += names_length;
            }
            uint32_t hard_dynamic_count = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->hard_dynamic.resize(hard_dynamic_count);
            for(uint32_t i=0; i<hard_dynamic_count; i++) {
                uint32_t hard_dynamic_length = *reinterpret_cast<uint32_t *>(data);
                data += sizeof(uint32_t);
                this->hard_dynamic[i].assign(data, hard_dynamic_length);
                data += hard_dynamic_length;
            }
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace outer {
    struct silly1 {
        uint32_t val1;
        uint32_t val2;
    }
}

namespace messages {
    enum MyFancyEnum{
        VALUE_1 = -3,
        VALUE_2,
        VALUE_3 = 32,
    }
    struct inctype {
        short_string sensor_name = "somename";
        uint32_t val = 10;
    }
    struct naked_struct @naked {
        uint32_t value1;
        uint32_t value2;
    }
    struct image {
        short_string sensor_name;
        uint32_t rows;
        uint32_t cols = 3;
        int32_t format;
        int32_t type;
        double timestamp = 1.300000;
        uint8_t pixels[3072];
        double neginit = -1.300000;
        double dexpinit = 2.518519;
        int32_t iexpinit = 2076;
        MyFancyEnum fancy;
        outer::silly1 sill;
        inctype includes;
        naked_struct nstruct;
    }
    struct complex_thing {
        int32_t one_val;
        MyFancyEnum fancy2;
        int32_t dynamic_array[];
        int32_t compact_array[100] @compact;
        string name;
        image img[3];
        outer::silly1 sill;
        string names[5];
        string hard_dynamic[];
    }
    struct fourmegs {
        uint32_t data[1048576];
    }
    struct getslarge {
        fourmegs vec[];
    }
}

)CBUF_CODE";

    };

    struct ATTR_PACKED fourmegs {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            sizeof(fourmegs),
            0x1C561D8C18C569A6,
            0.0,
            };
        uint32_t data[1048576];
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0x1C561D8C18C569A6;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "messages::fourmegs";
        static bool is_simple() { return true; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            for(int data_index = 0; data_index < 1048576; data_index++) {
                data[data_index] = 0;
            }
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
        }

        size_t encode_size() const
        {
            return sizeof(fourmegs);
        }

        void free_encode(const char *) const {}

        bool encode(char *data, unsigned int buf_size) const
        {
            if (buf_size < sizeof(fourmegs)) return false;
            memcpy(data, this, sizeof(*this));
            return true;
        }

        // This variant allows for no copy when writing to disk.
        const char *encode() const
        {
            return reinterpret_cast<const char *>(this);
        }

        bool decode(char *data, unsigned int buf_size)
        {
            if (buf_size < sizeof(fourmegs)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            memcpy(this, data, sizeof(*this));
            return true;
        }

        static bool decode(char *data, unsigned int buf_size, fourmegs** var)
        {
            if (buf_size < sizeof(fourmegs)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            *var = reinterpret_cast<fourmegs *>(data);
            return true;
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += sizeof(uint32_t) * 1048576;
            return ret_size;
        }

        bool encode_net(char *data, unsigned int buf_size) const
        {
            preamble.setSize(uint32_t(encode_net_size()));
            if (buf_size < preamble.size()) return false;
            *reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = preamble.size_;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint64_t *>(data) = preamble.hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<double *>(data) = preamble.packet_timest;
            data += sizeof(double);
            memcpy(data, this->data, 1048576*sizeof(uint32_t));
            data += 1048576*sizeof(uint32_t);
            
            return true;
        }

        bool decode_net(char *data, unsigned int buf_size)
        {
            uint32_t magic = *reinterpret_cast<uint32_t *>(data);
            if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();
            preamble.magic = magic;
            data += sizeof(uint32_t);
            uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);
            preamble.size_ = dec_size;
            if (preamble.size() > buf_size) CBUF_RETURN_FALSE();
            data += sizeof(uint32_t);
            uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);
            if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();
            data += sizeof(uint64_t);
            preamble.packet_timest = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            memcpy(this->data, data, 1048576*sizeof(uint32_t));
            data += 1048576*sizeof(uint32_t);
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace outer {
    struct silly1 {
        uint32_t val1;
        uint32_t val2;
    }
}

namespace messages {
    enum MyFancyEnum{
        VALUE_1 = -3,
        VALUE_2,
        VALUE_3 = 32,
    }
    struct inctype {
        short_string sensor_name = "somename";
        uint32_t val = 10;
    }
    struct naked_struct @naked {
        uint32_t value1;
        uint32_t value2;
    }
    struct image {
        short_string sensor_name;
        uint32_t rows;
        uint32_t cols = 3;
        int32_t format;
        int32_t type;
        double timestamp = 1.300000;
        uint8_t pixels[3072];
        double neginit = -1.300000;
        double dexpinit = 2.518519;
        int32_t iexpinit = 2076;
        MyFancyEnum fancy;
        outer::silly1 sill;
        inctype includes;
        naked_struct nstruct;
    }
    struct complex_thing {
        int32_t one_val;
        MyFancyEnum fancy2;
        int32_t dynamic_array[];
        int32_t compact_array[100] @compact;
        string name;
        image img[3];
        outer::silly1 sill;
        string names[5];
        string hard_dynamic[];
    }
    struct fourmegs {
        uint32_t data[1048576];
    }
    struct getslarge {
        fourmegs vec[];
    }
}

)CBUF_CODE";

    };

    struct ATTR_PACKED getslarge {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            0,
            0x37D4002E7F4A4EFF,
            0.0,
            };
        std::vector< fourmegs > vec;
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0x37D4002E7F4A4EFF;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "messages::getslarge";
        static bool is_simple() { return false; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            vec.clear();
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
            fourmegs::handle_metadata(fn, ctx);
        }

        size_t encode_size() const
        {
            return encode_net_size();
        }

        void free_encode(char *p) const
        {
            free(p);
        }

        bool encode(char *data, unsigned int buf_size) const
        {
            return encode_net(data, buf_size);
        }

        char *encode() const
        {
            size_t __struct_size = encode_size();
            preamble.setSize(uint32_t(__struct_size));
            char *data = reinterpret_cast<char *>(malloc(__struct_size));
            encode(data, __struct_size);
            return data;
        }

        bool decode(char *data, unsigned int buf_size)
        {
            return decode_net(data, buf_size);
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += vec[0].encode_net_size() * vec.size();
            ret_size += sizeof(uint32_t); // Encode the length of vec
            return ret_size;
        }

        bool encode_net(char *data, unsigned int buf_size) const
        {
            preamble.setSize(uint32_t(encode_net_size()));
            if (buf_size < preamble.size()) return false;
            *reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = preamble.size_;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint64_t *>(data) = preamble.hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<double *>(data) = preamble.packet_timest;
            data += sizeof(double);
            *reinterpret_cast<uint32_t *>(data) = uint32_t(this->vec.size());
            data += sizeof(uint32_t);
            memcpy(data, &this->vec[0], vec.size()*this->vec[0].encode_net_size());
            data += this->vec.size()*this->vec[0].encode_net_size();
            
            return true;
        }

        bool decode_net(char *data, unsigned int buf_size)
        {
            uint32_t magic = *reinterpret_cast<uint32_t *>(data);
            if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();
            preamble.magic = magic;
            data += sizeof(uint32_t);
            uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);
            preamble.size_ = dec_size;
            if (preamble.size() > buf_size) CBUF_RETURN_FALSE();
            data += sizeof(uint32_t);
            uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);
            if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();
            data += sizeof(uint64_t);
            preamble.packet_timest = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            uint32_t vec_count = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->vec.resize(vec_count);
            for(uint32_t i=0; i<vec_count; i++) {
                if (!this->vec[i].decode_net(data, buf_size)) CBUF_RETURN_FALSE();
                data += this->vec[i].preamble.size();
            }
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace outer {
    struct silly1 {
        uint32_t val1;
        uint32_t val2;
    }
}

namespace messages {
    enum MyFancyEnum{
        VALUE_1 = -3,
        VALUE_2,
        VALUE_3 = 32,
    }
    struct inctype {
        short_string sensor_name = "somename";
        uint32_t val = 10;
    }
    struct naked_struct @naked {
        uint32_t value1;
        uint32_t value2;
    }
    struct image {
        short_string sensor_name;
        uint32_t rows;
        uint32_t cols = 3;
        int32_t format;
        int32_t type;
        double timestamp = 1.300000;
        uint8_t pixels[3072];
        double neginit = -1.300000;
        double dexpinit = 2.518519;
        int32_t iexpinit = 2076;
        MyFancyEnum fancy;
        outer::silly1 sill;
        inctype includes;
        naked_struct nstruct;
    }
    struct complex_thing {
        int32_t one_val;
        MyFancyEnum fancy2;
        int32_t dynamic_array[];
        int32_t compact_array[100] @compact;
        string name;
        image img[3];
        outer::silly1 sill;
        string names[5];
        string hard_dynamic[];
    }
    struct fourmegs {
        uint32_t data[1048576];
    }
    struct getslarge {
        fourmegs vec[];
    }
}

)CBUF_CODE";

    };

};

