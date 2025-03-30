/**
 * \file src/plugins/output/json-kafka/src/pattern-matching/HyperscanCppInterface.hh
 * \author Rasa Oskuei <r.oskuei@gmail.com>
 * \brief This file is CPP wrapper of hyperscan C interface (header file)
 * \date 2023
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef HYPERSCAN_CPP_INTERFACE_H
#define HYPERSCAN_CPP_INTERFACE_H

#include <functional>
#include <utility>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include <iostream>

#include <hs/hs.h>
#include <hs/hs_runtime.h>
#include <hs/hs_compile.h>

namespace PatternMatching {

template <typename UserData>
class HyperscanCppInterface
{
  template <typename CallbackRefT>
  struct MatchingContext
  {
    CallbackRefT callback;
    UserData &user_data;
  };

  struct Pattern
  {
    std::string m_pattern;
    std::size_t m_pattern_id;
    unsigned m_flag;

    Pattern(const std::string &pattern,
            const std::size_t pattern_id,
            const unsigned flag) : m_pattern(pattern),
                                   m_pattern_id(pattern_id),
                                   m_flag(flag)
    {}
  };

  std::unique_ptr<hs_database_t, decltype(&hs_free_database)> m_database;
  std::unique_ptr<hs_scratch_t, decltype(&hs_free_scratch)> m_scratch;
  std::vector<Pattern> m_patterns;

  bool m_is_database_up_to_date;

  std::unique_ptr<hs_database_t, decltype(&hs_free_database)> compile_new_database()
  {
    std::vector<const char *> patterns_cstr;
    patterns_cstr.reserve(m_patterns.size());

    for (const auto &pattern : m_patterns)
      patterns_cstr.push_back(pattern.m_pattern.c_str());

    std::vector<unsigned> flags;
    flags.reserve(m_patterns.size());
    for (const auto &pattern : m_patterns)
      flags.push_back(pattern.m_flag);

    std::vector<unsigned int> ids;
    flags.reserve(m_patterns.size());
    for (const auto &pattern : m_patterns)
      ids.push_back(pattern.m_pattern_id);

    return std::unique_ptr<hs_database_t, decltype(&hs_free_database)>(compile_database(HS_MODE_BLOCK,
                                                                                        patterns_cstr, ids, flags),
                                                                       &hs_free_database);
  }

  void add_pettern_and_flags(const std::string &pattern_str, const std::size_t pattern_id)
  {
    if (pattern_str.empty())
      return;

    if (pattern_str[0] != '/')
    {
      static constexpr auto error_message = "no leading '/' char";
      throw std::runtime_error(error_message);
    }

    const std::size_t flags_start = pattern_str.find_last_of('/');

    if (flags_start == std::string::npos)
    {
      static constexpr auto error_message = "no trailing '/' char";
      throw std::runtime_error(error_message);
    }

    const unsigned flag = parse_flags(pattern_str.substr(flags_start + 1, pattern_str.size() - flags_start));

    m_patterns.emplace_back(pattern_str.substr(1, flags_start - 1), pattern_id, flag);
  }

  static unsigned parse_flags(const std::string &flags_str)
  {
    unsigned flags = 0;

    for (const auto &flag : flags_str)
      switch (flag)
      {
      case 'i':
        flags |= HS_FLAG_CASELESS;
        break;
      case 'm':
        flags |= HS_FLAG_MULTILINE;
        break;
      case 's':
        flags |= HS_FLAG_DOTALL;
        break;
      case 'a':
        flags |= HS_FLAG_ALLOWEMPTY;
        break;
      default: {
        const std::string error_message = std::string("Unsupported flag \'") + flag + "\'";
        throw std::runtime_error(error_message);
        break;
      }
      }

    return flags;
  }

  static hs_scratch_t *allocate_scratch(const hs_database_t *m_database)
  {
    hs_scratch_t *m_scratch = nullptr;
    const hs_error_t err = hs_alloc_scratch(m_database, &m_scratch);

    if (err != HS_SUCCESS)
    {
      const std::string error_message("Could not allocate m_scratch space. Exiting.");
      throw std::runtime_error(error_message);
    }
    return m_scratch;
  }

  static hs_database_t *compile_database(const std::uint32_t hs_mode,
                                         const std::vector<const char *> &cstr_patterns,
                                         const std::vector<unsigned int> &ids,
                                         const std::vector<unsigned> &flags)
  {
    if (cstr_patterns.empty())
      return nullptr;

    hs_database_t *m_database;
    hs_compile_error_t *compile_err;

    const hs_error_t err = hs_compile_multi(cstr_patterns.data(), flags.data(), ids.data(),
                                            cstr_patterns.size(), hs_mode, nullptr, &m_database, &compile_err);

    std::unique_ptr<hs_compile_error_t, decltype(&hs_free_compile_error)> compile_err_ptr(compile_err, &hs_free_compile_error);

    if (err != HS_SUCCESS)
    {
      if (compile_err->expression < 0)
        throw std::runtime_error(compile_err->message);
      else
      {
        const std::string error_message = std::string("Pattern ") +
                                          std::string(cstr_patterns[compile_err->expression]) +
                                          std::string(" failed compilation with error: ") + compile_err->message;
        throw std::runtime_error(error_message);
      }
    }

    return m_database;
  }

  template <typename CallbackRefT>
  static int matching_callback(const unsigned int id,
                               const unsigned long long from,
                               const unsigned long long to,
                               const unsigned int,
                               void *const type_erased_context)
  {
    const auto &context = *reinterpret_cast<MatchingContext<CallbackRefT> *>(type_erased_context);
    return static_cast<int>(context.callback(id, from, to, context.user_data));
  }

public:
  HyperscanCppInterface() : m_database(nullptr, &hs_free_database),
                            m_scratch(nullptr, &hs_free_scratch),
                            m_is_database_up_to_date(true)
  {}

  void register_pattern(const std::string &pattern, const std::size_t pattern_id)
  {
    m_is_database_up_to_date = false;
    add_pettern_and_flags(pattern, pattern_id);
  }

  void update_database()
  {
    if (m_is_database_up_to_date)
      return;

    m_database = compile_new_database();
    m_scratch.reset(allocate_scratch(m_database.get()));

    m_is_database_up_to_date = true;
  }

  template <typename CallbackRefT>
  void block_scan(const char *const sequence, const std::size_t sequence_len, CallbackRefT &&callback, UserData &user_data)
  {
    if (!this->m_database.get())
      return;

    MatchingContext<CallbackRefT &&> context{ std::forward<CallbackRefT>(callback), user_data };

    const auto scan_error = hs_scan(m_database.get(), sequence, sequence_len, 0, m_scratch.get(),
                                    matching_callback<CallbackRefT &&>, &context);

    if (scan_error != HS_SUCCESS && scan_error != HS_SCAN_TERMINATED)
      std::cerr << "hs_scan error, ERROR: Unable to block scan packet. Exiting. (" << scan_error << ")" << std::endl;
  }
};

} // namespace PatternMatching

#endif