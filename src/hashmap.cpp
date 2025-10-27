#include <unordered_map>
#include <string_view>
#include <set>
#include <vector>
#include <Rinternals.h>
#include <R.h>
#ifndef R_NO_REMAP
#define R_NO_REMAP
#endif 

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

/**
 * Auto-growing List based on LIST SEXP
 * The list (and thus its elements) is protected 
 * from garbage collection via R_PreserveObject(...)
 */
class R_List {
    private:
    SEXP data_;
    std::size_t idx_ = 0;
    RSerializer serializer;

    public:
    R_List() {
        data_ = PROTECT(Rf_allocVector(VECSXP, 16));
        R_PreserveObject(data_);
        UNPROTECT(1);
    }

    R_List(const R_List &other) : data_(other.data_), idx_(other.idx_), serializer(other.serializer) {}

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
    
    std::size_t size() const {
        return idx_;
    }

    SEXP operator[](std::size_t idx) const {
        return VECTOR_ELT(data_, idx);
    }

    void push_back(SEXP x) {
        std::size_t n = Rf_length(data_); // min length = 16
        if (idx_ >= n) {
            SEXP new_data = PROTECT(Rf_allocVector(VECSXP, n << 1));
            R_PreserveObject(new_data);
            UNPROTECT(1);

            // copy old elems 
            for (std::size_t i = 0; i < idx_; i++) {
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
    
    void compact() {
        
        if (_keys.size() <= 2 * _map.size()) {
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
        Hashmap *map = static_cast<Hashmap*>(GET_PTR(extptr));
        delete map;
    }

    public:
    
    Hashmap(const Hashmap &other) : _map(other._map), _keys(other._keys), _values(other._values) {}

    Hashmap() {}

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
        UNPROTECT(1);
        return extptr;
    }

    SEXP set(SEXP _key, SEXP _value, SEXP _replace) {
        SEXP k_dup = PROTECT(duplicate(_key));
        SEXP v_dup = PROTECT(duplicate(_value));
        _keys.push_back(k_dup);
        _values.push_back(v_dup);
        
        UNPROTECT(2);
        if (R_isTRUE(_replace)) {
            this->_map.insert_or_assign(k_dup, v_dup);
        } else {
            this->_map.insert({k_dup, v_dup});
        }
        this->compact();
        return R_NilValue;
    }   

    SEXP contains(SEXP _key) {
        return Rf_ScalarLogical(this->_map.contains(_key));
    }

    SEXP contains_range(SEXP _keys) {
        std::size_t len = Rf_length(_keys);
        SEXP list = PROTECT(Rf_allocVector(LGLSXP, len));

        for (std::size_t i = 0; i < len; i++) {
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
        std::size_t size = Rf_length(_keys);
        SEXP list = PROTECT(Rf_allocVector(VECSXP, size));
        for (std::size_t i = 0; i < size; i++) {
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
        std::size_t size = Rf_length(_keys); 
        for (std::size_t i = 0; i < size; i++) {
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
        std::size_t len = Rf_length(_keys);
        for (std::size_t i = 0; i < len; i++) {
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
        std::size_t i = 0;
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
        std::size_t i = 0;
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

    // returns new, inverted hashmap
    /*
    Invariants:
    - _duplicates %in% c("stack", "first")
    */
    SEXP invert(SEXP _duplicates) {
        Hashmap *map_inverted = new Hashmap;

        if (strcmp(CHAR(Rf_asChar(_duplicates)), "stack") == 0) {
            std::unordered_map<SEXP, R_List, sexp_hash, sexp_eq> value_to_keys;

            for (const auto& [k, v] : _map) {
                value_to_keys[v].push_back(k);
            }

            for (const auto& [new_key, old_keys] : value_to_keys) {
                if (old_keys.size() == 1) {
                    map_inverted->set(new_key, old_keys[0], Rf_ScalarLogical(0));
                } else {
                    SEXP key_list = PROTECT(allocVector(VECSXP, old_keys.size()));
                    for (std::size_t i = 0; i < old_keys.size(); i++) {
                        SET_VECTOR_ELT(key_list, i, old_keys[i]);
                    }
                    map_inverted->set(new_key, key_list, Rf_ScalarLogical(0));
                    UNPROTECT(1);
                }
            }
            // first
        } else {
            for (const auto& [k, v] : _map) {
                map_inverted->_map.emplace(v, k);
            }
        }
        return map_inverted->to_extptr();
    }

    SEXP clone() {
        Hashmap* m = new Hashmap(*this);
        return m->to_extptr();
    }

    /*
    Preconditions:
    - length(list) == 2
    - names(list[1] == "keys")
    - names(list[2] == "values")
    - typeof(list[1]) == "list"
    - typeof(list[2]) == "list"
    - length(list[1]) == length(list[2])
    */
    SEXP from_list(SEXP list) {
        SEXP keys = VECTOR_ELT(list, 0);
        SEXP values = VECTOR_ELT(list, 1);
        this->clear();
        this->set_range(keys, values, Rf_ScalarLogical(0));
        return R_NilValue;
    }
};

extern "C" {
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
    return map->set_range(_keys, _values, _replace);
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

SEXP C_hashmap_invert(SEXP _map, SEXP _duplicates) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->invert(_duplicates); 
}

SEXP C_hashmap_clone(SEXP _map) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->clone(); 
}

SEXP C_hashmap_fromlist(SEXP _map, SEXP _list) {
    Hashmap *map = static_cast<Hashmap*>(GET_PTR(_map));
    return map->from_list(_list); 
}
}
