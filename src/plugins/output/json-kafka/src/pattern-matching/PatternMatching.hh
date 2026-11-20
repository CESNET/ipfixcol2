/**
 * \file src/plugins/output/json-kafka/src/pattern-matching/PatternMatching.hh
 * \author Rasa Oskuei <r.oskuei@gmail.com>
 * \brief Pattern matching abstaction (header file)
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

#ifndef PATTERN_MATCHING_H
#define PATTERN_MATCHING_H

#include <utility>
#include <cstdlib>
#include <limits>
#include <vector>
#include <memory>
#include <string>

#include "HyperscanCppInterface.hh"

namespace PatternMatching {

template <typename UserData>
class PatternMatching
{
  struct CallbackBase
  {
    virtual std::size_t operator()(const std::size_t,
                                   const std::size_t,
                                   const std::size_t,
                                   UserData &) = 0;
    virtual ~CallbackBase() = default;
  };

  template <typename FunctorType>
  class Callback : public CallbackBase
  {
    using NonReferenceFunctorType = typename std::remove_reference<FunctorType>::type;
    typename std::conditional<std::is_function<NonReferenceFunctorType>::value,
                              FunctorType, NonReferenceFunctorType>::type functor;

  public:
    template <typename Arg>
    Callback(Arg &&arg) : functor{ std::forward<Arg>(arg) }
    {}

    virtual std::size_t operator()(const std::size_t pattern_id,
                                   const std::size_t from,
                                   const std::size_t to,
                                   UserData &user_data) override final
    {
      return functor(pattern_id, from, to, user_data);
    }
  };

  std::vector<std::unique_ptr<CallbackBase>> m_callbacks;

  static constexpr std::size_t get_pattern_id_max_bits()
  {
    return 12;
  }

  static constexpr std::size_t get_pattern_id(const std::size_t id)
  {
    return id & ((1 << get_pattern_id_max_bits()) - 1);
  }

  static constexpr std::size_t get_callback_id(const std::size_t id)
  {
    return (id >> get_pattern_id_max_bits());
  }

protected:
  HyperscanCppInterface<UserData> hyperscan;

  bool call_callbacks(const std::size_t id,
                      const std::size_t from,
                      const std::size_t to,
                      UserData &user_data) const
  {
    const std::size_t callback_id = get_callback_id(id);
    const std::size_t module_pattern_id = get_pattern_id(id);
    return static_cast<bool>((*m_callbacks[callback_id].get())(module_pattern_id, from, to, user_data));
  }

public:
  template <typename CallbackT>
  std::size_t register_callback(CallbackT &&functor)
  {
    m_callbacks.emplace_back(new Callback<CallbackT>(std::forward<CallbackT>(functor)));

    return m_callbacks.size() - 1;
  }

  void register_pattern(const std::string &pattern, const std::size_t pattern_id, const std::size_t callback_id)
  {
    if (pattern_id >> get_pattern_id_max_bits())
      throw std::runtime_error("pattern_id exceeds the maximum possible value.");

    if (((std::numeric_limits<unsigned int>::max() - pattern_id) >> get_pattern_id_max_bits()) < callback_id)
      throw std::runtime_error("Hyperscan pattern_id exceeds the maximum possible value.");

    hyperscan.register_pattern(pattern, (callback_id << get_pattern_id_max_bits()) | pattern_id);
  }

  void update_database()
  {
    hyperscan.update_database();
  }

  void match_pattern(const char *const sequence, const std::size_t sequence_len, UserData &user_data)
  {
    hyperscan.block_scan(
      sequence, sequence_len,
      [this](const std::size_t id,
             const std::size_t from,
             const std::size_t to,
             UserData &user_data) { return this->call_callbacks(id, from, to, user_data); },
      user_data);
  }
};

} // namespace PatternMatching

#endif