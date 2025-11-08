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
    // Constructor
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

    // Copy & move constructor
    RSerializer(const RSerializer& other) : RSerializer() {}
    RSerializer(const RSerializer&& other) : RSerializer() {}

    std::string_view serialize(SEXP obj) {
        buffer_.clear();
        R_Serialize(obj, &stream);

        return std::string_view(
            reinterpret_cast<const char*>(buffer_.data()), buffer_.size()
        );
    }
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

    public:
    R_List() {
        data_ = PROTECT(Rf_allocVector(VECSXP, 16));
        R_PreserveObject(data_);
        UNPROTECT(1);
    }

    R_List(const R_List &other) :  idx_(other.idx_) {
        data_ = PROTECT(Rf_shallow_duplicate(other.data_));
        R_PreserveObject(data_);
        UNPROTECT(1);
    }

    // move assignment
    R_List& operator=(R_List&& other) {
        if (this != &other) {
            R_ReleaseObject(data_);
            data_ = other.data_; // data still protected
            idx_ = other.idx_;
            other.data_ = R_NilValue;
        }
        return *this;
    }
    
    // copy assignment
    R_List& operator=(const R_List& other) {
        if (this != &other) {
            R_ReleaseObject(data_);
            data_ = PROTECT(Rf_shallow_duplicate(other.data_));
            idx_ = other.idx_;
            R_PreserveObject(data_);
            UNPROTECT(1);
        }
        return *this;
    }

    ~R_List() noexcept {
        if (data_ != R_NilValue) {
            R_ReleaseObject(data_);
        }
    }
    
    std::size_t size() const noexcept {
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

class Hashmap {

    private:
        std::unordered_map<SEXP, SEXP, sexp_hash, sexp_eq> map_;
        R_List keys_;
        R_List values_;
    
    void insert(SEXP key, SEXP value) {
        keys_.push_back(key);
        values_.push_back(value);
        map_.emplace(key, value);
    }

    void compact() {
        
        if (keys_.size() <= 2 * map_.size()) {
            return;
        }
        R_List new_keys;
        R_List new_values;

        for (const auto& [k, v] : map_) {
             new_keys.push_back(k);
             new_values.push_back(v);
        }
        keys_ = std::move(new_keys);
        values_ = std::move(new_values);
    }

    // finalizer
    static void finalizer(SEXP extptr) {
        Hashmap *map = static_cast<Hashmap*>(GET_PTR(extptr));
        delete map;
    }

    public:

    Hashmap(const Hashmap &other) : map_(other.map_), keys_(other.keys_), values_(other.values_) {}

    Hashmap() {}

    SEXP to_list() const {

        SEXP list = PROTECT(Rf_allocVector(VECSXP, 2));
        SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
        
        SET_VECTOR_ELT(list, 0, keys());
        SET_VECTOR_ELT(list, 1, values());

        SET_STRING_ELT(names, 0, mkChar("keys"));
        SET_STRING_ELT(names, 1, mkChar("values"));

        setAttrib(list, R_NamesSymbol, names);
        UNPROTECT(2);
        return list;
    }

    // creates an R external pointer to the class instance
    SEXP to_extptr() const {
        Hashmap* ptr = const_cast<Hashmap*>(this);
        SEXP extptr = PROTECT(R_MakeExternalPtr(ptr, R_NilValue, R_NilValue));
        R_RegisterCFinalizerEx(extptr, finalizer, TRUE);
        UNPROTECT(1);
        return extptr;
    }

    SEXP set(SEXP key, SEXP value, SEXP replace) {
        keys_.push_back(key);
        values_.push_back(value);
        
        if (R_isTRUE(replace)) {
            this->map_.insert_or_assign(key, value);
        } else {
            this->map_.try_emplace(key, value);
        }
        this->compact();
        return R_NilValue;
    }   

    SEXP contains(SEXP key) const {
        return Rf_ScalarLogical(this->map_.contains(key));
    }

    SEXP contains_range(SEXP keys) const {
        std::size_t len = Rf_length(keys);
        SEXP list = PROTECT(Rf_allocVector(LGLSXP, len));

        for (std::size_t i = 0; i < len; i++) {
            SEXP k = VECTOR_ELT(keys, i);
            LOGICAL(list)[i] = this->map_.contains(k);
        }
        UNPROTECT(1);
        return list;
    }

    SEXP get(SEXP key) const {
        auto it = this->map_.find(key);
        if (it != this->map_.end()) {
            return it->second;
        } else {
            return R_NilValue;
        }
    }

    /**
     * @brief lookup multiple keys and return each value as an element in a list
     * @cond _keys must be a list. This should be checked in the caller of get_range
     */
    SEXP get_range(SEXP keys) const {
        std::size_t size = Rf_length(keys);
        SEXP list = PROTECT(Rf_allocVector(VECSXP, size));
        for (std::size_t i = 0; i < size; i++) {
            SEXP k = VECTOR_ELT(keys, i);
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
    SEXP set_range(SEXP keys, SEXP values, SEXP replace) {

        std::size_t size = Rf_length(keys); 

        if (R_isTRUE(replace)) {
            for (std::size_t i = 0; i < size; i++) {
                SEXP k = VECTOR_ELT(keys, i);
                SEXP v = VECTOR_ELT(values, i);
                keys_.push_back(k);
                values_.push_back(v);
                this->map_.insert_or_assign(k, v);
            }
        } else {
            for (std::size_t i = 0; i < size; i++) {
                SEXP k = VECTOR_ELT(keys, i);
                SEXP v = VECTOR_ELT(values, i);
                keys_.push_back(k);
                values_.push_back(v);
                this->map_.emplace(k, v);
            }
        }
        this->compact();  
        return R_NilValue;
    }

    SEXP remove(SEXP key) {
        this->map_.erase(key);
        this->compact();
        return R_NilValue;
    }

    SEXP remove_range(SEXP keys) {
        std::size_t len = Rf_length(keys);
        for (std::size_t i = 0; i < len; i++) {
            SEXP k = VECTOR_ELT(keys, i);
            this->map_.erase(k);
        }
        this->compact();
        return R_NilValue;
    }

    /**
     * @brief Return all keys inside the map as an R List
     */
    SEXP keys() const {
        // create list
        SEXP list = PROTECT(allocVector(VECSXP, this->map_.size()));
        std::size_t i = 0;
        for (auto it = this->map_.begin(); it != this->map_.end(); ++it) {
            SET_VECTOR_ELT(list, i++, it->first);
        }
        UNPROTECT(1);
        return list;
    }

    /**
     * @brief Return all values inside the map as an R List
     */
    SEXP values() const {
        SEXP list = PROTECT(allocVector(VECSXP, this->map_.size()));
        std::size_t i = 0;
        for (auto it = this->map_.begin(); it != this->map_.end(); ++it) {
            SET_VECTOR_ELT(list, i++, it->second);
        }
        UNPROTECT(1);
        return list;
    }

    SEXP size() const noexcept{
        return Rf_ScalarReal(map_.size());
    }

    /**
     * @brief clears the map
     */
    SEXP clear() noexcept {
        map_.clear();
        keys_ = R_List();
        values_ = R_List();
        return R_NilValue;
    }

    // returns new, inverted hashmap
    /*
    Invariants:
    - _duplicates %in% c("stack", "first")
    */
    SEXP invert(SEXP duplicates) {
        Hashmap *map_inverted = new Hashmap;
        
        // stack
        if (strcmp(CHAR(Rf_asChar(duplicates)), "stack") == 0) {
            std::unordered_map<SEXP, std::vector<SEXP>, sexp_hash, sexp_eq> value_to_keys;

            for (const auto& [k, v] : map_) {
                value_to_keys[v].push_back(k);
            }

            for (const auto& [new_key, old_keys] : value_to_keys) {
                if (old_keys.size() == 1) {
                    map_inverted->insert(new_key, old_keys[0]);
                } else {
                    SEXP key_list = PROTECT(allocVector(VECSXP, old_keys.size()));
                    for (std::size_t i = 0; i < old_keys.size(); i++) {
                        SET_VECTOR_ELT(key_list, i, old_keys[i]);
                    }
                    map_inverted->insert(new_key, key_list);
                    UNPROTECT(1);
                }
            }
            // first
        } else {
            for (const auto& [k, v] : map_) {
                map_inverted->insert(v, k);
            }
        }
        return map_inverted->to_extptr();
    }

    SEXP clone() const {
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

    SEXP C_hashmap_init() {
    Hashmap *map = new Hashmap;
    return map->to_extptr();
}

SEXP C_hashmap_set(SEXP map, SEXP key, SEXP value, SEXP replace) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->set(key, value, replace);
    }

SEXP C_hashmap_get(SEXP map, SEXP key) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->get(key);
}

SEXP C_hashmap_remove(SEXP map, SEXP key) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->remove(key);
}

SEXP C_hashmap_getkeys(SEXP map) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->keys();
}

SEXP C_hashmap_getvals(SEXP map) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->values();
}

SEXP C_hashmap_clear(SEXP map) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    map_->clear();
    return R_NilValue;
}
SEXP C_hashmap_size(SEXP map) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->size();
}

SEXP C_hashmap_set_range(SEXP map, SEXP keys, SEXP values, SEXP replace) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->set_range(keys, values, replace);
}

SEXP C_hashmap_contains(SEXP map, SEXP key) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->contains(key);
}

SEXP C_hashmap_contains_range(SEXP map, SEXP key) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->contains_range(key);
}

SEXP C_hashmap_get_range(SEXP map, SEXP keys) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->get_range(keys);
}

SEXP C_hashmap_remove_range(SEXP map, SEXP keys) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->remove_range(keys);
}

SEXP C_hashmap_tolist(SEXP map) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->to_list();
}

SEXP C_hashmap_invert(SEXP map, SEXP _duplicates) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->invert(_duplicates); 
}

SEXP C_hashmap_clone(SEXP map) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->clone(); 
}

SEXP C_hashmap_fromlist(SEXP map, SEXP list) {
    Hashmap *map_ = static_cast<Hashmap*>(GET_PTR(map));
    return map_->from_list(list); 
}
}
