#pragma once
#include <functional>
#include <tuple>

namespace Fwog::detail::hashing
{
  template<typename T>
  struct hash;

  template<class T>
  inline void hash_combine(std::size_t& seed, const T& v)
  {
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // Recursive template code derived from Matthieu M.
  template<class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
  struct HashValueImpl
  {
    static void apply(size_t& seed, const Tuple& tuple)
    {
      HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
      hash_combine(seed, std::get<Index>(tuple));
    }
  };

  template<class Tuple>
  struct HashValueImpl<Tuple, 0>
  {
    static void apply(size_t& seed, const Tuple& tuple)
    {
      hash_combine(seed, std::get<0>(tuple));
    }
  };

  template<typename... TT>
  struct hash<std::tuple<TT...>>
  {
    size_t operator()(const std::tuple<TT...>& tt) const
    {
      size_t seed = 0;
      HashValueImpl<std::tuple<TT...>>::apply(seed, tt);
      return seed;
    }
  };
} // namespace Fwog::detail::hashing
