// Copyright (c) by respective owners including Yahoo!, Microsoft, and
// individual contributors. All rights reserved. Released under a BSD (revised)
// license as described in the file LICENSE.
#pragma once

#include "io_buf.h"
#include "cache.h"
#include <set>
#include <map>
#include <queue>

#include <fmt/format.h>
#include <type_traits>

namespace VW
{
namespace model_utils
{
namespace details
{
inline size_t check_length_matches(size_t actual_len, size_t expected_len)
{
  if (expected_len > 0 && actual_len != expected_len) { THROW("Unexpected end of file encountered."); }
  return actual_len;
}

template <typename T>
size_t write_text_mode_output(io_buf& io, const T& var, const std::string& name_or_readable_field_template)
{
  if (name_or_readable_field_template.empty()) { return 0; }

  std::string message;
  // If the user has supplied a template string then use that.
  if (name_or_readable_field_template.find("{}") != VW::string_view::npos)
  { message = fmt::format(name_or_readable_field_template, var); }
  else
  {
    // Use the default template string.
    message = fmt::format("{} = {}\n", name_or_readable_field_template, var);
  }

  return details::check_length_matches(io.bin_write_fixed(message.c_str(), message.size()), message.size());
}
}  // namespace details

/**
 * @brief This function is the uniform interface reading a variable from the model file. The value must be a POD type.
 *
 * @tparam T type of value to serialize or deserialize. This is important as it determines the number of bytes to be
 * processed.
 * @param io model file io_buf to read from or write to
 * @param var Variable to write into for the deserialized value. Must be a type which supports
 * validly bit casting to a sequence of bytes. Does not need to be initialized.
 * @return size_t the number of bytes read
 */
template <typename T,
    typename std::enable_if<!std::is_pointer<T>::value && std::is_trivially_copyable<T>::value && std::is_pod<T>::value,
        bool>::type = true>
size_t read_model_field(io_buf& io, T& var)
{
  auto* data = reinterpret_cast<char*>(&var);
  auto len = sizeof(var);

  return details::check_length_matches(io.bin_read_fixed(data, len), len);
}

/**
 * @brief This function is the uniform interface for serializing a variable to
 * the model file. The value must be a POD type.
 *
 * @tparam T type of value to serialize or deserialize. This is important as it determines the number of bytes to be
 * processed.
 * @param io model file io_buf to read from or write to
 * @param var value to serialize. Must be a type which supports
 * validly bit casting to a sequence of bytes. Must be initialized.
 * @param name_or_readable_field_template (Only used when text == true) The name
 * of the variable or a template string to use when writing.
 *   - If empty, then no output is written for this call
 *   - If the string contains a "{}", then the variable value will be substituted for that. Note: more than one "{}" in
 * the string will result in a runtime error due to the templating failure.
 *   - Otherwise, if the string is non-empty and does not contain a "{}" the value will be used as the name of the
 * variable and be outputted in the form `("{} = {}\n", name_or_readable_field_template, var)`
 * @param text whether to write in text mode or binary mode.
 * @return size_t the number of bytes written
 */
template <typename T,
    typename std::enable_if<!std::is_pointer<T>::value && std::is_trivially_copyable<T>::value && std::is_pod<T>::value,
        bool>::type = true>
size_t write_model_field(io_buf& io, const T& var, const std::string& name_or_readable_field_template, bool text)
{
  if (text) { return details::write_text_mode_output(io, var, name_or_readable_field_template); }

  const auto* data = reinterpret_cast<const char*>(&var);
  auto len = sizeof(var);

  // If not text we are just writing the binary data.
  return details::check_length_matches(io.bin_write_fixed(data, len), len);
}

template <typename T>
size_t read_model_field(io_buf& io, std::set<T>& set)
{
  size_t bytes = 0;
  uint64_t set_size;
  bytes += read_model_field(io, set_size);
  for (uint64_t i = 0; i < set_size; ++i)
  {
    T v;
    bytes += read_model_field(io, v);
    set.insert(v);
  }
  return bytes;
}

template <typename T>
size_t write_model_field(io_buf& io, const std::set<T>& set, const std::string& upstream_name, bool text)
{
  if (upstream_name.find("{}") != std::string::npos) { THROW("Field template not allowed for set."); }
  size_t bytes = 0;
  uint64_t set_size = static_cast<uint64_t>(set.size());
  bytes += write_model_field(io, set_size, upstream_name + ".size()", text);
  uint64_t i = 0;
  for (const T& v : set)
  {
    bytes += write_model_field(io, v, fmt::format("{}[{}]", upstream_name, i), text);
    ++i;
  }
  return bytes;
}

template <typename T>
size_t read_model_field(io_buf& io, std::vector<T>& vec)
{
  size_t bytes = 0;
  uint64_t vec_size;
  bytes += read_model_field(io, vec_size);
  for (uint64_t i = 0; i < vec_size; ++i)
  {
    T v;
    bytes += read_model_field(io, v);
    vec.push_back(v);
  }
  return bytes;
}

template <typename T>
size_t write_model_field(io_buf& io, const std::vector<T>& vec, const std::string& upstream_name, bool text)
{
  if (upstream_name.find("{}") != std::string::npos) { THROW("Field template not allowed for vector."); }
  size_t bytes = 0;
  uint64_t vec_size = static_cast<uint64_t>(vec.size());
  bytes += write_model_field(io, vec_size, upstream_name + ".size()", text);
  for (uint64_t i = 0; i < vec_size; ++i)
  { bytes += write_model_field(io, vec[i], fmt::format("{}[{}]", upstream_name, i), text); }
  return bytes;
}

template <typename F, typename S>
size_t read_model_field(io_buf& io, std::pair<F, S>& pair)
{
  size_t bytes = 0;
  bytes += read_model_field(io, pair.first);
  bytes += read_model_field(io, pair.second);
  return bytes;
}

template <typename F, typename S>
size_t write_model_field(io_buf& io, const std::pair<F, S>& pair, const std::string& upstream_name, bool text)
{
  if (upstream_name.find("{}") != std::string::npos) { THROW("Field template not allowed for pair."); }
  size_t bytes = 0;
  bytes += write_model_field(io, pair.first, upstream_name + ".first", text);
  bytes += write_model_field(io, pair.second, upstream_name + ".second", text);
  return bytes;
}

template <typename T>
size_t read_model_field(io_buf& io, std::priority_queue<T>& pq)
{
  size_t bytes = 0;
  uint64_t queue_size;
  bytes += read_model_field(io, queue_size);
  for (uint64_t i = 0; i < queue_size; ++i)
  {
    T v;
    bytes += read_model_field(io, v);
    pq.push(v);
  }
  return bytes;
}

template <typename T>
size_t write_model_field(io_buf& io, const std::priority_queue<T>& pq, const std::string& upstream_name, bool text)
{
  if (upstream_name.find("{}") != std::string::npos) { THROW("Field template not allowed for priority_queue."); }
  std::priority_queue<T> pq_cp = pq;
  size_t bytes = 0;
  uint64_t queue_size = static_cast<uint64_t>(pq_cp.size());
  bytes += write_model_field(io, queue_size, upstream_name + ".size()", text);
  uint64_t i = 0;
  while (!pq_cp.empty())
  {
    const T& v = pq_cp.top();
    bytes += write_model_field(io, v, fmt::format("{}[{}]", upstream_name, i), text);
    pq_cp.pop();
    ++i;
  }
  return bytes;
}

template <typename K, typename V>
size_t read_model_field(io_buf& io, std::map<K, V>& map)
{
  size_t bytes = 0;
  uint64_t map_size;
  bytes += read_model_field(io, map_size);
  for (uint64_t i = 0; i < map_size; ++i)
  {
    std::pair<K, V> pair;
    bytes += read_model_field(io, pair);
    map[pair.first] = pair.second;
  }
  return bytes;
}

template <typename K, typename V>
size_t write_model_field(io_buf& io, const std::map<K, V>& map, const std::string& upstream_name, bool text)
{
  if (upstream_name.find("{}") != std::string::npos) { THROW("Field template not allowed for map."); }
  size_t bytes = 0;
  uint64_t map_size = static_cast<uint64_t>(map.size());
  bytes += write_model_field(io, map_size, upstream_name + ".size()", text);
  uint64_t i = 0;
  for (const auto& pair : map)
  {
    bytes += write_model_field(io, pair.first, fmt::format("{}.key{}", upstream_name, i), text);
    bytes += write_model_field(io, pair.second, fmt::format("{}[{}]", upstream_name, pair.first), text);
    ++i;
  }
  return bytes;
}

}  // namespace model_utils
}  // namespace VW
