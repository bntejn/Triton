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

      std::unordered_map<std::string, std::shared_ptr<Tag>> Tag::tagMap = std::unordered_map<std::string, std::shared_ptr<Tag>>();

      Tag::Tag(const char* data) {
        this->data = std::string(data);
      }

      Tag::Tag(std::string data) {
        this->data = data;
      }

      Tag::Tag(const Tag& tag) {
        tag.getData();
      }

      std::shared_ptr<Tag> Tag::getTag(const std::string& data) {
        auto tagpair = Tag::tagMap.find(data);
        if (tagpair != Tag::tagMap.end()) {
          return (*tagpair).second;
        } else {
          auto newTag = std::make_shared<Tag>(Tag(data));
          Tag::tagMap.insert(std::pair<std::string, std::shared_ptr<Tag>>(data, newTag));
          return newTag;
        }
      }

      std::shared_ptr<Tag> Tag::getTag(const char *data) {
        return Tag::getTag(std::string(data));
      }

      Tag::~Tag() {
        /* the shared pointer `this->data` shall not be deleted. */
      }

      std::string Tag::getData() const {
        return this->data;
      }

      /*
      bool Tag::operator<(const std::shared_ptr<Tag>& rhs) const {
         // pointer-based comparison. cheaper than string comparison
         return this->data < rhs.data;
      }
      bool Tag::operator==(const std::shared_ptr<Tag>& rhs) const {
        // pointer-based comparison
        return this->data == rhs.data;
      }
      */

    }; /* taint namespace */
  }; /* engines namespace */
}; /* triton namespace */

