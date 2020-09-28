// Copyright (c) 2014-2018, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <INode.h>

#include <CryptoNoteCore/Currency.h>

#include <Global/Constants.h>

namespace CryptoNote
{
class BlockchainDB;

class Hardfork
{
public:
    typedef enum
    {
        LikelyForked,
        UpdateNeeded,
        Ready,
    } State;

    /*!
     * @brief creates a new Hardfork object
     *
     * @param originalVersion           the block version of blocks 0 through to the first fork
     * @param windowSize                the size of the window in block to consider for version voting
     * @param defaultThresholdPercent   the size of the majority in percents
     */
    Hardfork(BlockchainDB &db,
             uint8_t originalVersion = 1,
             uint64_t originalVersionTillHeight = Qwertycoin::Hardfork::DEFAULT_ORIGINAL_VERSION_TILL_HEIGHT,
             uint64_t windowSize = Qwertycoin::Hardfork::DEFAULT_WINDOW_SIZE,
             uint8_t defaultThresholdPercent = Qwertycoin::Hardfork::DEFAULT_THRESHOLD_PERCENT);

    uint8_t getBlockVote(CryptoNote::Block const *b) const;
    uint8_t getBlockVersion(CryptoNote::Block const &b) const;

    /*!
     * @brief add a new hardfork height
     *
     * returns true if no error, false otherwise
     *
     * @param version       the major block version for the fork
     * @param height        the height the hardfork takes effect
     * @param threshold     the threshold of votes needed for this fork (0-100)
     */
    bool addFork(uint8_t const &version,
                 uint64_t const &height,
                 uint8_t const &threshold);

    /*!
     * @brief add a new hardfork height
     *
     * returns true if no error, false otherwise
     *
     * @param version       the major block version for the fork
     * @param height        the height the hardfork takes effect
     */
    bool addFork(uint8_t const &version,
                 uint64_t const &height);

    /*!
     * @brief initialize the object
     *
     * Must be done after adding all the required hardforks via add above
     */
    void init();

    /*!
     * @brief check whether a new block would be accepted
     *
     * returns true if the block is accepted, false otherwise
     *
     * @param block         the new block
     *
     * This check is made by add. It is exposed publicly to allow
     * the caller to inexpensively check whether a block would be
     * accepted or rejected by its version number. Indeed, if this
     * check could only be done as part of add, the caller would
     * either have to add the block to the blockchain first, then
     * call add, then have to pop the block from the blockchain if
     * its version did not satisfy the hard fork requirements, or
     * call add first, then, if the hard fork requirements are met,
     * add the block to the blockchain, upon which a failure (the
     * block being invalid, double spending, etc) would cause the
     * hardfork object to reorganize.
     */
    bool check(const CryptoNote::Block &block) const;

    /*!
     * @brief same as check, but for a particular height, rather than the top
     *
     * NOTE: this does not play well with voting, and relies on voting to be
     * disabled (that is, forks happen on the scheduled date, whether or not
     * enough blocks have voted for the fork).
     *
     * returns true if no error, false otherwise
     *
     * @param block         the new block
     * @param height        which height to check for
     */
    bool checkForHeight(const CryptoNote::Block &block, uint64_t const &height) const;

    /*!
     * @brief add a new block
     *
     * returns true if no error, false otherwise
     *
     * @param block         the new block
     */
    bool add(const CryptoNote::Block &block, uint64_t const &height);

    /*!
     * @brief called when the blockchain is reorganized
     *
     * This will rescan the blockchain to determine which hard forks
     * have been triggered
     *
     * returns true if no error, false otherwise
     *
     * @param blockchain    the blockchain
     * @param height        of the last block kept from the previous blockchain
     */
    bool reorganizeFromBlockHeight(uint64_t const &height);
    bool reorganizeFromChainHeight(uint64_t const &height);

    State getState(uint64_t const &height) const;
    State getState() const;

    /*!
     * @brief returns the hard fork version for the given block height
     *
     * @param height        height of the block to check
     */
    uint8_t get(uint64_t const &height);

    /*!
     * @brief returns the latest "ideal" version
     *
     * This is the latest version that's been scheduled
     */
    uint8_t getIdealVersion() const;

    /*!
     * @brief returns the "ideal" version for a given height
     *
     * @param height        height of the block to check
     */
    uint8_t getIdealVersion(uint64_t const &height) const;

    /*!
     * @brief returns the next version
     *
     * This is the version which will we fork to next
     */
    uint8_t getNextVersion() const;

    /*!
     * @brief returns the current version
     *
     * This is the latest version that's past its trigger date and had enough votes
     * at one point in the past.
     */
    uint8_t getCurrentVersion() const;

    /*!
     * @brief returns the earliest block a given version may activate
     */
    uint64_t getEarliestIdealHeightForVersion(uint8_t version) const;

    /*!
     * @brief returns information about current voting state
     *
     * returns true if the given version is enabled (ie, the current version
     * is at least the passed version), false otherwise
     *
     * @param version           the version to check voting for
     * @param window            the number of blocks considered in voting
     * @param votes             number of votes for next version
     * @param threshold         number of votes needed to switch to next version
     * @param earliestHeight    earliest height at which the version can take effect
     */
    bool getVotingInfo(uint8_t const &version,
                       uint32_t const &window,
                       uint32_t &votes,
                       uint32_t &threshold,
                       uint64_t &earliestHeight,
                       uint8_t &voting) const;

    /*!
     * @brief returns the size of the voting window in blocks
     */
    uint64_t getWindowSize() const
    {
        return windowSize;
    }

private:
    bool add(uint8_t const &blockVersion, uint8_t votingVersion, uint64_t const &height);
    bool doCheck(uint8_t const &blockVersion, uint8_t votingVersion) const;
    bool doCheckForHeight(uint8_t const &blockVersion, uint8_t votingVersion, uint64_t const &height) const;
    bool rescanFromBlockHeight(uint64_t const &height);
    bool rescanFromChainHeight(uint64_t const &height);

    int getVotedForkIndex(uint64_t const &height) const;

    uint8_t getBlockVersion(uint64_t const &height) const;
    uint8_t getEffectiveVersion(uint8_t votingVersion) const;

private:
    BlockchainDB &db;

    /*!
     * count of the block versions in the last N blocks
     */
    unsigned int lastVersions[256];

    /*!
     * rolling window of the last N blocks' versions
     */
    std::deque<uint8_t> versions;
    uint8_t defaultThresholdPercent;
    uint8_t originalVersion;

    uint32_t currentForkIndex;

    uint64_t originalVersionTillHeight;
    uint64_t windowSize;

    struct Params
    {
        uint8_t version;
        uint8_t threshold;
        uint64_t height;
        Params(uint8_t const &version,
               uint64_t const &height,
               uint8_t const &threshold)
                : version(version),
                  threshold(threshold),
                  height(height)
        {}
    };
    std::vector<Params> heights;
};
}
