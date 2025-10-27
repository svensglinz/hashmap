#include <unordered_map>
#include <string_view>
#include <stdint.h> 
#include <vector>
#include <Rinternals.h>
#include <R.h>

#ifndef R_NO_REMAP
#define R_NO_REMAP
#endif 

#undef length
#define GET_PTR(SEXP) (R_ExternalPtrAddr(SEXP))

class RSerializer {
    private:
    std::vector<unsigned char> buffer_;
    R_outpstream_st stream;

    static void write_callback(R_outpstream_st* stream, void* data, int length)  {
        auto *vec = static_cast<std::vector<unsigned char>*>(stream->data);
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        vec->insert(vec->end(), bytes, bytes + length);
    }

    public:
    RSerializer() {
        buffer_.reserve(1 << 12);
        R_InitOutPStream(
            &stream,
            reinterpret_cast<R_pstream_data_t>(&buffer_),
            R_pstream_binary_format,
            3,
            nullptr,
            write_callback,
            nullptr,
            R_NilValue
        );
    }

    std::string_view serialize(SEXP obj) {
        buffer_.clear();
        R_Serialize(obj, &stream);

        return std::string_view(
            reinterpret_cast<const char*>(buffer_.data()), buffer_.size()
        );
    }

    const std::vector<unsigned char>& buffer() const {return buffer_;}
};


struct sexp_eq {
    bool operator() (SEXP a, SEXP b) const {
        return R_compute_identical(a, b, 0);
    }
};

// hash function
struct sexp_hash {
    mutable RSerializer serializer;
    std::size_t operator()(SEXP e) const {
        std::string_view bytes = serializer.serialize(e);
        return std::hash<std::string_view>{}(bytes);
    }
};

class R_List {
    private:
    SEXP data_;
    int idx_ = 0;
    RSerializer serializer;

    public:
    R_List() {
        data_ = PROTECT(Rf_allocVector(VECSXP, 16));
        R_PreserveObject(data_);
        UNPROTECT(1);
    }

    // move assignment
    R_List& operator=(R_List&& other) {
        if (this != &other) {
            R_ReleaseObject(data_);
            data_ = other.data_;
            idx_ = other.idx_;
            other.data_ = R_NilValue;
        }
        return *this;
    }
    
    // copy assignment
    R_List& operator=(const R_List& other) {
        if (this != &other) {
            R_ReleaseObject(data_);
            data_ = PROTECT(duplicate(other.data_));
            idx_ = other.idx_;
            R_PreserveObject(data_);
            UNPROTECT(1);
        }
        return *this;
    }

    ~R_List() {
        if (data_ != R_NilValue) {
            R_ReleaseObject(data_);
        }
    }
    
    SEXP data() {
        return data_;
    }

    int size() {
        return idx_;
    }

    void push_back(SEXP x) {
        int n = Rf_length(data_); // min length = 16
        if (idx_ >= n) {
            SEXP new_data = PROTECT(Rf_allocVector(VECSXP, n << 1));
            R_PreserveObject(new_data);
            UNPROTECT(1);

            // copy old elems 
            for (int i = 0; i < idx_; i++) {
                SET_VECTOR_ELT(new_data, i, VECTOR_ELT(data_, i));
            }
            R_ReleaseObject(data_);
            data_ = new_data;
        }
        // copy in new value
        SET_VECTOR_ELT(data_, idx_, x);
        idx_++;
    }
};

// do a prototype with things we can inherit ? (ir )
class Hashmap {

    private:
        std::unordered_map<SEXP, SEXP, sexp_hash, sexp_eq> _map;
        R_List _keys;
        R_List _values;
        RSerializer serializer;
    
    void compact() {
        
        if (_keys.size() <= 4 * _map.size()) {
            return;
        }
        R_List new_keys;
        R_List new_values;

        for (const auto& [k, v] : _map) {
             new_keys.push_back(k);
             new_values.push_back(v);
        }
        _keys = std::move(new_keys);
        _values = std::move(new_values);
    }

    // finalizer here
    static void finalizer(SEXP extptr) {

    }

    public:

    SEXP to_list() {

        SEXP list = PROTECT(Rf_allocVector(VECSXP, 2));
        SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));

        // duplicate before ?
        SET_VECTOR_ELT(list, 0, keys());
        SET_VECTOR_ELT(list, 1, values());

        SET_STRING_ELT(names, 0, mkChar("keys"));
        SET_STRING_ELT(names, 1, mkChar("values"));

        setAttrib(list, R_NamesSymbol, names);
        UNPROTECT(2);
        return list;
    }

    // creates an R external pointer to the class instance
    SEXP to_extptr() {
        SEXP extptr = PROTECT(R_MakeExternalPtr(this, R_NilValue, R_NilValue));
        R_RegisterCFinalizerEx(extptr, finalizer, TRUE);
        setAttrib(extptr, R_ClassSymbol, mkString("hashmap"));
        UNPROTECT(1);
        return extptr;
    }

    SEXP set(SEXP _key, SEXP _value, SEXP replace) {
        SEXP k_dup = PROTECT(duplicate(_key));
        SEXP v_dup = PROTECT(duplicate(_value));

        _keys.push_back(k_dup);
        _values.push_back(v_dup);
        
        UNPROTECT(2);
        if (R_isTRUE(replace)) {
            this->_map.insert_or_assign(k_dup, v_dup);
        } else {
            this->_map.emplace(k_dup, v_dup);
        }
        this->compact();
        return R_NilValue;
    }   

    SEXP contains(SEXP _key) {
        return Rf_ScalarLogical(this->_map.contains(_key));
    }

    SEXP contains_range(SEXP _keys) {
        int len = Rf_length(_keys);
        SEXP list = PROTECT(Rf_allocVector(LGLSXP, len));

        for (int i = 0; i < len; i++) {
            SEXP k = VECTOR_ELT(_keys, i);
            LOGICAL(list)[0] = this->_map.contains(k);
        }
        UNPROTECT(1);
        return list;
    }

    SEXP get(SEXP _key) {

        auto it = this->_map.find(_key);
        if (it != this->_map.end()) {
            return it->second;
        } else {
            return R_NilValue;
        }
    }


    /**
     * @brief lookup multiple keys and return each value as an element in a list
     * @cond _keys must be a list. This should be checked in the caller of get_range
     */
    SEXP get_range(SEXP _keys) {
        int size = Rf_length(_keys);
        SEXP list = PROTECT(Rf_allocVector(VECSXP, size));
        for (int i = 0; i < size; i++) {
            SEXP k = VECTOR_ELT(_keys, i);
            SEXP v = this->get(k);
            SET_VECTOR_ELT(list, i, v);
        }
        UNPROTECT(1);
        return list;
    }

    /**
     * @brief insert multiple key, value pairs
     * @cond _keys, _values must be a list and length(_keys) == length(_values)
     * This should be checked in the caller of insert_range
     */
    SEXP set_range(SEXP _keys, SEXP _values, SEXP _replace) {
        int size = Rf_length(_keys); 
        for (int i = 0; i < size; i++) {
            SEXP k = VECTOR_ELT(_keys, i);
            SEXP v = VECTOR_ELT(_values, i);
            this->set(k, v, _replace);
        }
        return R_NilValue;
    }

    SEXP remove(SEXP _key) {
        this->_map.erase(_key);
        this->compact();
        return R_NilValue;
    }

    SEXP remove_range(SEXP _keys) {
        int len = Rf_length(_keys);
        for (int i = 0; i < len; i++) {
            SEXP k = VECTOR_ELT(_keys, i);
            this->remove(k);
        }
        return R_NilValue;
    }

    /**
     * @brief Return all keys inside the map as an R List
     */
    SEXP keys() {
        // create list
        SEXP list = PROTECT(allocVector(VECSXP, this->_map.size()));
        int i = 0;
        for (auto it = this->_map.begin(); it != this->_map.end(); ++it) {
            SET_VECTOR_ELT(list, i++, it->first);
        }
        UNPROTECT(1);
        return list;
    }

    /**
     * @brief Return all values inside the map as an R List
     */
    SEXP values() {
        SEXP list = PROTECT(allocVector(VECSXP, this->_map.size()));
        int i = 0;
        for (auto it = this->_map.begin(); it != this->_map.end(); ++it) {
            SET_VECTOR_ELT(list, i++, it->second);
        }
        UNPROTECT(1);
        return list;
    }

    SEXP size() {
        return Rf_ScalarReal(_map.size());
    }

    /**
     * @brief clears the map
     */
    SEXP clear() {
        _map.clear();
        return R_NilValue;
    }

    // fn is callback
    SEXP for_each(SEXP fn) {
        int size = _map.size();
        SEXP res = PROTECT(Rf_allocVector(VECSXP, size));
        
        int i = 0;
        for (const auto& [k, v] : _map) {
            SEXP call = PROTECT(Rf_lang3(fn, k, v));
            SEXP result = PROTECT(Rf_eval(call, R_GlobalEnv));
            SET_VECTOR_ELT(res, i, result);
            UNPROTECT(2);
        }
        UNPROTECT(1);
        return res;
    }

    // returns new, inverted hashmap
    SEXP invert() {
        Hashmap *map_inverted = new Hashmap;
        for (const auto& [k, v] : _map) {
            map_inverted->set(v, k, Rf_ScalarLogical(1)); // option to stack (list), / use first match / use last match
        }
        return map_inverted->to_extptr();
    }
};


class MultiHashMap {
    std::unordered_multimap<SEXP, SEXP, sexp_hash, sexp_eq> _map;


};

extern "C" {
// called by garbage collector to free hahsmap
void C_hashmap_finalize(SEXP map_ptr){
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(map_ptr));
    delete map;
}

// initialize empty hashmap
SEXP C_hashmap_init() {
    Hashmap *map = new Hashmap;
    return map->to_extptr();
}

SEXP C_hashmap_set(SEXP _map, SEXP _key, SEXP _value, SEXP _replace) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->set(_key, _value, _replace);
    }

SEXP C_hashmap_get(SEXP _map, SEXP _key) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->get(_key);
}

SEXP C_hashmap_remove(SEXP _map, SEXP _key) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->remove(_key);
}

SEXP C_hashmap_getkeys(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->keys();
}

SEXP C_hashmap_getvals(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->values();
}

SEXP C_hashmap_clear(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    map->clear();
    return R_NilValue;
}
SEXP C_hashmap_size(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->size();
}

SEXP C_hashmap_set_range(SEXP _map, SEXP _keys, SEXP _values, SEXP _replace) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->set(_keys, _values, _replace);
}

SEXP C_hashmap_contains(SEXP _map, SEXP _key) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->contains(_key);
}

SEXP C_hashmap_contains_range(SEXP _map, SEXP _key) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->contains_range(_key);
}

SEXP C_hashmap_get_range(SEXP _map, SEXP _keys) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->get_range(_keys);
}

SEXP C_hashmap_remove_range(SEXP _map, SEXP _keys) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->remove_range(_keys);
}

SEXP C_hashmap_tolist(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->to_list();
}

SEXP C_hashmap_invert(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->invert(); 
}
}
