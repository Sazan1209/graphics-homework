#ifndef ALIGNSTD430_HPP
#define ALIGNSTD430_HPP

#include "tuple_tools.hpp"
#include <cstddef>
#include <glm/glm.hpp>
#include <cstdlib>
#include <utility>

/*
1. If the member is a scalar consuming N basic machine units, the base align-
ment is N .
2. If the member is a two- or four-component vector with components consum-
ing N basic machine units, the base alignment is 2N or 4N , respectively.
3. If the member is a three-component vector with components consuming N
basic machine units, the base alignment is 4N .
4. If the member is an array of scalars or vectors, the base alignment and array
stride are set to match the base alignment of a single array element, according
to rules (1), (2), and (3). The
array may have padding at the end; the base offset of the member following
the array is rounded up to the next multiple of the base alignment.
5. If the member is a column-major matrix with C columns and R rows, the
matrix is stored identically to an array of C column vectors with R compo-
nents each, according to rule (4).
6. If the member is an array of S column-major matrices with C columns and
R rows, the matrix is stored identically to a row of S × C column vectors
with R components each, according to rule (4).
7. If the member is a row-major matrix with C columns and R rows, the matrix
is stored identically to an array of R row vectors with C components each,
according to rule (4).
8. If the member is an array of S row-major matrices with C columns and R
rows, the matrix is stored identically to a row of S × R row vectors with C
components each, according to rule (4).
9. If the member is a structure, the base alignment of the structure is N , where
N is the largest base alignment value of any of its members. The individual members of this sub-
structure are then assigned offsets by applying this set of rules recursively,
where the base offset of the first member of the sub-structure is equal to the
aligned offset of the structure. The structure may have padding at the end;
the base offset of the member following the sub-structure is rounded up to
the next multiple of the base alignment of the structure.
10. If the member is an array of S structures, the S elements of the array are laid
out in order, according to rule (9).
*/

namespace detail
{

constexpr size_t round_up_to(size_t val, size_t to)
{
  return ((val - 1 + to) / to) * to;
}

// clang-format off
template <typename T>
concept glsl_scalar =
    std::same_as<T, glm::uint> ||
    std::same_as<T, int> ||
    std::same_as<T, float> ||
    std::same_as<T, double>;

template <typename T>
concept glsl_vec2 =
    std::same_as<T, glm::bvec2> ||
    std::same_as<T, glm::ivec2> ||
    std::same_as<T, glm::dvec2> ||
    std::same_as<T, glm::vec2>;

template <typename T>
concept glsl_vec3 =
    std::same_as<T, glm::bvec3> ||
    std::same_as<T, glm::ivec3> ||
    std::same_as<T, glm::dvec3> ||
    std::same_as<T, glm::vec3>;

template <typename T>
concept glsl_vec4 =
    std::same_as<T, glm::bvec4> ||
    std::same_as<T, glm::ivec4> ||
    std::same_as<T, glm::dvec4> ||
    std::same_as<T, glm::vec4>;

// imat, bmat, umat don't exist apparently. Which is nice
template <typename T>
concept glsl_mat =
    std::same_as<T, glm::mat2x2> ||
    std::same_as<T, glm::mat2x3> ||
    std::same_as<T, glm::mat3x2> ||
    std::same_as<T, glm::mat3x3> ||
    std::same_as<T, glm::dmat2x2> ||
    std::same_as<T, glm::dmat2x3> ||
    std::same_as<T, glm::dmat3x2> ||
    std::same_as<T, glm::dmat3x3>;

// clang-format on

template <typename T>
concept glsl_vec = glsl_vec2<T> || glsl_vec3<T> || glsl_vec4<T>;

template <typename T>
concept glsl_vec_or_scal = glsl_vec<T> || glsl_scalar<T>;

///////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
constexpr size_t baseAlign = 0;

template <glsl_scalar T>
constexpr size_t baseAlign<T> = sizeof(T);

template <glsl_vec2 T>
constexpr size_t baseAlign<T> = baseAlign<typename T::value_type> * 2;

template <glsl_vec4 T>
constexpr size_t baseAlign<T> = baseAlign<typename T::value_type> * 4;

template <glsl_vec3 T>
constexpr size_t baseAlign<T> = baseAlign<typename T::value_type> * 4;

// The alignment for arrays of scalars/vectors, arrays of matrices and arrays of structs work
// exactly the same in practice, even though there are separate rules for them
template <typename T, size_t N>
constexpr size_t baseAlign<T[N]> = baseAlign<T>;

template <glsl_mat T>
constexpr size_t baseAlign<T> = baseAlign<typename T::row_type>;

template <typename... Ts>
constexpr size_t max_size(Ts... ts)
{
  size_t res = 0;
  size_t vals[] = {ts...};
  for (size_t i = 0; i < sizeof...(Ts); ++i)
  {
    res = std::max(res, vals[i]);
  }
  return res;
}

template <typename Tup, size_t... indices>
constexpr size_t tuple_align_max(std::integer_sequence<size_t, indices...>)
{
  return max_size(baseAlign<std::tuple_element_t<indices, Tup>>...);
}

template <typename T>
struct BaseAlign_struct_helper
{
  using Tup = decltype(to_tuple(std::declval<T>()));
  constexpr static size_t LEN = std::tuple_size_v<Tup>;
  constexpr static size_t val = tuple_align_max<Tup>(std::make_integer_sequence<size_t, LEN>());
};


template <typename T>
  requires(!glsl_mat<T> && !glsl_scalar<T> && !glsl_vec<T>)
constexpr size_t baseAlign<T> = BaseAlign_struct_helper<T>::val;

///////////////////////////////////////////////////////////////////////////////////////////////////////////

template <glsl_vec_or_scal T>
size_t memcpy_std430(std::byte* buf, size_t offset, T& val)
{
  offset = round_up_to(offset, baseAlign<T>);
  memcpy(buf + offset, &val, sizeof(val)); // Probably illegal
  return offset + sizeof(val);             // No padding at the end
}

template <typename T, size_t N>
using Arr = T[N];

template <typename T, size_t N>
size_t memcpy_std430(std::byte* buf, size_t offset, Arr<T, N>& arr)
{
  for (size_t i = 0; i < N; ++i)
  {
    offset = memcpy_std430(buf, offset, arr[i]);
    offset = round_up_to(offset, baseAlign<T[N]>); // Includes padding
  }
  return offset;
}

template <glsl_mat T>
size_t memcpy_std430(std::byte* buf, size_t offset, T& val)
{
  typename T::row_type tmp[T::length()];
  for (size_t i = 0; i < T::length(); ++i)
  {
    tmp[i] = val[static_cast<typename T::length_type>(i)]; // I hate MSVC and it's damned warnings
  }
  return memcpy_std430(buf, offset, tmp);
}

template <typename... Ts, size_t... indices>
size_t memcpy_std430_tup(
  std::byte* buf, size_t offset, std::tuple<Ts...>& val, std::integer_sequence<size_t, indices...>);

template <typename T>
size_t memcpy_std430(std::byte* buf, size_t offset, T& val)
{
  offset = round_up_to(offset, baseAlign<T>);
  auto tup = to_tuple(val);
  offset = memcpy_std430_tup(
    buf, offset, tup, std::make_integer_sequence<size_t, std::tuple_size_v<decltype(tup)>>());
  return round_up_to(offset, baseAlign<T>); // includes padding
}

template <typename T, typename... Ts>
size_t memcpy_std430(std::byte* buf, size_t offset, T& val, Ts&... ts)
{
  offset = memcpy_std430(buf, offset, val);
  return memcpy_std430(buf, offset, ts...);
}

template <typename... Ts, size_t... indices>
size_t memcpy_std430_tup(
  std::byte* buf, size_t offset, std::tuple<Ts...>& val, std::integer_sequence<size_t, indices...>)
{
  return memcpy_std430(buf, offset, std::get<indices>(val)...);
}

////////////////////////////////////////////////////////////////////////////////////

template <typename T>
constexpr size_t alignedSize = 0;

template <glsl_vec_or_scal T>
constexpr size_t alignedSize<T> = sizeof(T);

template <typename T, size_t N>
constexpr size_t alignedSize<T[N]> = round_up_to(alignedSize<T>, baseAlign<T>) * N;

template <glsl_mat T>
constexpr size_t alignedSize<T> = alignedSize<typename T::row_type[T::length()]>;

template <typename T>
constexpr size_t aligned_size_tup(size_t offset)
{
  offset = round_up_to(offset, baseAlign<T>);
  return alignedSize<T> + offset;
}

// T1 is necessary, because otherwise the call is ambiguous. Which is stupid
template <typename T, typename T1, typename... Ts>
constexpr size_t aligned_size_tup(size_t offset)
{
  offset = round_up_to(offset, baseAlign<T>);
  offset += alignedSize<T>;
  return aligned_size_tup<T1, Ts...>(offset);
}

template <typename... Ts>
constexpr size_t alignedSize<std::tuple<Ts...>> = aligned_size_tup<Ts...>(0);

template <typename T>
  requires(!glsl_mat<T> && !glsl_scalar<T> && !glsl_vec<T>)
constexpr size_t alignedSize<T> =
  round_up_to(alignedSize<decltype(to_tuple(std::declval<T>()))>, baseAlign<T>);


} // namespace detail

template <typename T>
constexpr size_t alignedSize = detail::alignedSize<T>;

template <typename T>
using AlignedBuffer = std::byte[alignedSize<T>];

template <typename T>
size_t memcpy_aligned_std430(AlignedBuffer<T>& buf, T& val)
{
  return detail::memcpy_std430(static_cast<std::byte*>(buf), 0, val);
}

#endif // ALIGNSTD430_HPP
