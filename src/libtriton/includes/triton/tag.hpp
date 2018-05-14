//! \file
/*
**  Copyright (C) - Triton
**
**  This program is under the terms of the BSD License.
*/

#ifndef TRITON_TAINTTAG_H
#define TRITON_TAINTTAG_H

#include <set>

#include <triton/memoryAccess.hpp>
#include <triton/register.hpp>
#include <triton/symbolicEngine.hpp>
#include <triton/tritonTypes.hpp>



//! The Triton namespace
namespace triton {
/*!
 *  \addtogroup triton
 *  @{
 */

  //! The Engines namespace
  namespace engines {
  /*!
   *  \ingroup triton
   *  \addtogroup engines
   *  @{
   */

    //! The Taint namespace
    namespace taint {
    /*!
     *  \ingroup engines
     *  \addtogroup taint
     *  @{
     */

      //! A wrapper class that encapsulates a tag data
      class Tag {
        private:
          std::string data;

          //! Initialize a Tag
          Tag(const char* data);

          //! Initialize a Tag
          Tag(std::string data);

        public:
          //! Copy constructor
          Tag(const Tag& tag);

          static std::unordered_map<std::string, std::shared_ptr<Tag>> tagMap;

          static std::shared_ptr<Tag> getTag(const std::string& data);

          static std::shared_ptr<Tag> getTag(const char *data);

          ~Tag();

          //! Retrieve the data
          std::string getData() const;

          //bool operator<(const Tag& rhs) const;

          //bool operator==(const Tag& rhs) const;
      };

    /*! @} End of taint namespace */
    };
  /*! @} End of engines namespace */
  };
/*! @} End of triton namespace */
};

#endif /* !__TRITON_TAINTENGINE_H__ */

