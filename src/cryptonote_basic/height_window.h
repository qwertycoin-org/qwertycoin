// Copyright (c) 2022-2026, The Qwertycoin Project

#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_config.h"

namespace cryptonote
{
  enum class height_window_verdict
  {
    absent,
    inside,
    early,
    expired,
    malformed
  };

  inline height_window_verdict check_height_window(
      const tx_extra_height_window& w, const uint64_t height, const uint64_t early_lead = 0) noexcept
  {
    if (w.from > w.until)
      return height_window_verdict::malformed;
    if (height > w.until)
      return height_window_verdict::expired;
    const uint64_t latest_from = height > std::numeric_limits<uint64_t>::max() - early_lead
        ? std::numeric_limits<uint64_t>::max()
        : height + early_lead;
    if (w.from > latest_from)
      return height_window_verdict::early;
    return height_window_verdict::inside;
  }

  inline height_window_verdict check_tx_height_window(
      const transaction_prefix& tx, const uint64_t height, tx_extra_height_window& w, const uint64_t early_lead = 0)
  {
    switch (get_tx_height_window(tx.extra, w))
    {
      case 0:
        return height_window_verdict::absent;
      case 1:
        if (tx.unlock_time >= CRYPTONOTE_MAX_BLOCK_NUMBER) // QWC-3 (b)
          return height_window_verdict::malformed;
        return check_height_window(w, height, early_lead);
      default:
        return height_window_verdict::malformed;
    }
  }

  inline height_window_verdict check_block_tx_height_window(
      const transaction_prefix& tx, const uint8_t block_major_version, const uint64_t height, tx_extra_height_window& w)
  {
    if (is_qwc_contracts_hardfork(block_major_version))
      return check_tx_height_window(tx, height, w);
    return get_tx_height_window(tx.extra, w) == 0 ? height_window_verdict::absent : height_window_verdict::malformed;
  }

  inline uint64_t height_window_effective_unlock(
      const uint64_t unlock_time, const uint64_t mined_height, const tx_extra_height_window& w) noexcept
  {
    if (unlock_time >= CRYPTONOTE_MAX_BLOCK_NUMBER)
      return unlock_time;
    const uint64_t delay = QWC_WINDOW_UNLOCK_DELAY_BLOCKS;
    if (mined_height > std::numeric_limits<uint64_t>::max() - delay || mined_height + delay < w.until)
      return unlock_time;
    const uint64_t cap = CRYPTONOTE_MAX_BLOCK_NUMBER - 1;
    const uint64_t locked = w.until > cap - delay ? cap : w.until + delay;
    return unlock_time > locked ? unlock_time : locked;
  }

  inline uint64_t tx_effective_unlock_time(const transaction_prefix& tx, const uint64_t mined_height)
  {
    if (tx.vin.empty() || tx.vin[0].type() == typeid(txin_gen))
      return tx.unlock_time;
    tx_extra_height_window w{};
    if (get_tx_height_window(tx.extra, w) != 1)
      return tx.unlock_time;
    return height_window_effective_unlock(tx.unlock_time, mined_height, w);
  }

  inline uint64_t height_window_default_until(const uint64_t tip_height) noexcept
  {
    const uint64_t step = QWC_WALLET_DEFAULT_WINDOW_ROUNDING;
    const uint64_t raw = tip_height + QWC_WALLET_DEFAULT_WINDOW_BLOCKS;
    return (raw + step - 1) / step * step;
  }
}
