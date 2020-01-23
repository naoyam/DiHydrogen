// @H2_LICENSE_TEXT@

#ifndef H2_META_TYPELIST_SIZE_HPP_
#define H2_META_TYPELIST_SIZE_HPP_

#include "Length.hpp"

namespace h2
{
namespace meta
{
namespace tlist
{
/** @brief Alias for LengthVT. */
template <typename List>
using SizeVT = LengthVT<List>;

/** @brief Get the index of a given type in the list. */
template <typename List>
constexpr unsigned long SizeV() { return SizeVT<List>::value; }

#ifndef H2_USE_CXX17
template <typename List>
inline constexpr unsigned long Size = SizeV<List>();
#endif // H2_USE_CXX17

}// namespace tlist
}// namespace meta
}// namespace h2
#endif // H2_META_TYPELIST_SIZE_HPP_