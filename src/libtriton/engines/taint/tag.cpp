//! \file
/*
**  Copyright (C) - Triton
**
**  This program is under the terms of the BSD License.
*/

#include <triton/exceptions.hpp>
#include <triton/tag.hpp>


namespace triton {
  namespace engines {
    namespace taint {

      std::unordered_map<std::string, Tag*> Tag::tagMap = std::unordered_map<std::string, Tag*>();

      Tag::Tag(const char* data) {
        this->data = std::string(data);
      }

      Tag::Tag(const Tag& tag) {
        this->data = tag.getData();
      }

      Tag* Tag::createTag(const char *data) {
        auto tagpair = Tag::tagMap.find(std::string(data));
        if (tagpair != Tag::tagMap.end()) {
          return (*tagpair).second;
        } else {
          Tag* newTag = new Tag(data);
          Tag::tagMap.insert(std::pair<std::string, Tag*>(std::string(data), newTag));
          return newTag;
        }
      }

      Tag::~Tag() {
        /* the shared pointer `this->data` shall not be deleted. */
      }

      std::string Tag::getData() const {
        return this->data;
      }

      bool Tag::operator<(const Tag &rhs) const {
        // pointer-based comparison. cheaper than string comparison
        return this->data < rhs.data;
      }

      bool Tag::operator==(const Tag &rhs) const {
        /* pointer-based comparison */
        return this->data == rhs.data;
      }

    }; /* taint namespace */
  }; /* engines namespace */
}; /* triton namespace */

