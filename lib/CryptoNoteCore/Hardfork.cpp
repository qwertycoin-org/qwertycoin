

#include <algorithm>
#include <cstdio>

#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/Hardfork.h>
#include <CryptoNoteCore/LMDB/BlockchainDB.h>
#include <CryptoNoteCore/LMDB/DatabaseLMDB.h>

namespace CryptoNote {
Hardfork::Hardfork(BlockchainDB &db,
                   uint8_t originalVersion,
                   uint64_t originalVersionTillHeight,
                   uint64_t windowSize,
                   uint8_t defaultThresholdPercent)
        : db(db),
          originalVersion(originalVersion),
          originalVersionTillHeight(originalVersionTillHeight)
{
    if (windowSize == 0) {
        throw "windowSize needs to be strictly positive";
    }

    if (defaultThresholdPercent > 100) {
        throw "defaultThresholdPercent needs to be between 0 and 100";
    }
}

uint8_t Hardfork::getBlockVote(const CryptoNote::Block *b) const
{
    return b->minorVersion;
}

uint8_t Hardfork::getBlockVersion(CryptoNote::Block const &b) const
{
    return b.majorVersion;
}

bool Hardfork::addFork(const uint8_t &version,
                       const uint64_t &height,
                       const uint8_t &threshold)
{
    // add in order
    if (version == 0) {
        return false;
    }

    if (!heights.empty()) {
        if (version <= heights.back().version) {
            return false;
        }

        if (height <= heights.back().height) {
            return false;
        }
    }

    if (threshold > 100) {
        return false;
    }

    heights.push_back(Params(version, height, threshold));

    return true;
}

bool Hardfork::addFork(const uint8_t &version, const uint64_t &height)
{
    return addFork(version, height, defaultThresholdPercent);
}

uint8_t Hardfork::getEffectiveVersion(uint8_t votingVersion) const
{
    if (!heights.empty()) {
        uint8_t const maxVersion = heights.back().version;
        if (votingVersion > maxVersion) {
            votingVersion = maxVersion;
        }
    }

    return votingVersion;
}

bool Hardfork::doCheck(uint8_t const &blockVersion, uint8_t votingVersion) const
{
    return blockVersion == heights[currentForkIndex].version &&
           votingVersion >= heights[currentForkIndex].version;
}

bool Hardfork::check(const CryptoNote::Block &block) const
{
    return doCheck(getBlockVersion(block), getBlockVote(&block));
}

bool Hardfork::doCheckForHeight(const uint8_t &blockVersion,
                                uint8_t votingVersion,
                                const uint64_t &height) const
{
    int forkIndex = getVotedForkIndex(height);
    return blockVersion == heights[forkIndex].version &&
           votingVersion >= heights[forkIndex].version;
}

bool Hardfork::checkForHeight(const CryptoNote::Block &block, const uint64_t &height) const
{
    return doCheckForHeight(getBlockVersion(block),
                            getBlockVote(&block),
                            height);
}

bool Hardfork::add(const uint8_t &blockVersion,
                   uint8_t votingVersion,
                   const uint64_t &height)
{
    if (!doCheck(blockVersion, votingVersion)) {
        return false;
    }

    db.setHardForkVersion(height, heights[currentForkIndex].version);

    while (versions.size() >= windowSize) {
        uint8_t const oldVersion = versions.front();
        assert(lastVersions[oldVersion] >= 1);
        lastVersions[oldVersion]--;
        versions.pop_front();
    }

    lastVersions[votingVersion]++;
    versions.push_back(votingVersion);

    uint8_t voted = getVotedForkIndex(height + 1);
    if (voted > currentForkIndex) {
        currentForkIndex = voted;
    }

    return true;
}

bool Hardfork::add(const CryptoNote::Block &block, const uint64_t &height)
{
    return add(getBlockVersion(block), getBlockVote(&block), height);
}

void Hardfork::init()
{
    // add a placeholder for the default version, to avoid special cases
    if (heights.empty()) {
        heights.push_back(Params(originalVersion, 0, 0));
    }

    versions.clear();
    for (size_t n = 0; n < 256; ++n) {
        lastVersions[n] = 0;
    }
    currentForkIndex = 0;

    // restore state from DB
    uint64_t height = db.height();
    if (height > windowSize) {
        height -= windowSize - 1;
    }

    bool populate = false;

    try {
        db.getHardForkVersion(0);
    } catch (...) {
        populate = true;
    }

    if (populate) {
        height = 1;
    }

    if (populate) {
        reorganizeFromChainHeight(height);
        db.setHardForkVersion(0, originalVersion);
    } else {
        rescanFromChainHeight(height);
    }
}

uint8_t Hardfork::getBlockVersion(const uint64_t &height) const
{
    if (height <= originalVersionTillHeight) {
        return originalVersion;
    }

    const CryptoNote::Block &block = db.getBlockFromHeight(height);

    return getBlockVersion(block);
}

bool Hardfork::reorganizeFromBlockHeight(const uint64_t &height)
{
    if (height >= db.height()) {
        return false;
    }

    db.setBatchTransactions(true);
    bool stopBatch = db.batchStart();

    versions.clear();

    for (size_t n = 0; n < 256; ++n) {
        lastVersions[n] = 0;
    }

    const uint64_t rescanHeight = height >= (windowSize - 1) ? height - (windowSize - 1) : 0;
    const uint8_t startVersion = height == 0 ? originalVersion : db.getHardForkVersion(height);
    while (currentForkIndex > 0 && heights[currentForkIndex].version > startVersion) {
        --currentForkIndex;
    }

    for (uint64_t h = rescanHeight; h <= height; ++h) {
        CryptoNote::Block bBlock = db.getBlockFromHeight(h);
        const uint8_t v = getEffectiveVersion(getBlockVersion(bBlock));
        lastVersions[v]++;
        versions.push_back(v);
    }

    uint8_t voted = getVotedForkIndex(height + 1);
    if (voted > currentForkIndex) {
        currentForkIndex = voted;
    }

    const uint64_t bcHeight = db.height();
    for (uint64_t h = height + 1; h < bcHeight; ++h) {
        add(db.getBlockFromHeight(h), h);
    }

    if (stopBatch) {
        db.batchStop();
    }

    return true;
}

bool Hardfork::reorganizeFromChainHeight(const uint64_t &height)
{
    if (height == 0) {
        return false;
    }

    return reorganizeFromBlockHeight(height);
}

bool Hardfork::rescanFromBlockHeight(const uint64_t &height)
{
    db.blockTxnStart(true);
    if (height >= db.height()) {
        db.blockTxnStop();
        return false;
    }

    versions.clear();

    for (size_t n = 0; n < 256; ++n) {
        lastVersions[n] = 0;
    }

    for (uint64_t h = height; h < db.height(); ++h) {
        CryptoNote::Block bBlock = db.getBlockFromHeight(h);
        const uint8_t v = getEffectiveVersion(getBlockVersion(bBlock));
        lastVersions[v]++;
        versions.push_back(v);
    }

    uint8_t lastV = db.getHardForkVersion(db.height() - 1);
    currentForkIndex = 0;
    while (currentForkIndex + 1 < heights.size() && heights[currentForkIndex].version != lastV) {
        ++currentForkIndex;
    }

    uint8_t voted = getVotedForkIndex(db.height());
    if (voted > currentForkIndex) {
        currentForkIndex = voted;
    }

    db.blockTxnStop();

    return true;
}

bool Hardfork::rescanFromChainHeight(const uint64_t &height)
{
    if (height == 0) {
        return false;
    }
    return rescanFromBlockHeight(height - 1);
}

int Hardfork::getVotedForkIndex(const uint64_t &height) const
{
    uint32_t accumulatedVotes = 0;
    for (unsigned int n = heights.size() - 1; n > currentForkIndex; --n) {
        uint8_t v = heights[n].version;
        accumulatedVotes += lastVersions[v];
        uint32_t threshold = (windowSize * heights[n].threshold + 99) / 100;

        if (height >= heights[n].height && accumulatedVotes >= threshold) {
            return n;
        }
    }

    return currentForkIndex;
}

Hardfork::State Hardfork::getState(const uint64_t &height) const
{
    // no hardforks setup yet
    if (height <= 1) {
        return Ready;
    }

    uint64_t tLastFork = heights.back().height;
    if (height < tLastFork) {
        return UpdateNeeded;
    } else if (height > tLastFork) {
        return LikelyForked;
    } else {
        return Ready;
    }
}

Hardfork::State  Hardfork::getState() const
{
    return getState(db.height());
}

uint8_t Hardfork::get(const uint64_t &height)
{
    if (height > db.height()) {
        assert(false);
        return 255;
    }

    if (height == db.height()) {
        return getCurrentVersion();
    }

    return db.getHardForkVersion(height);
}

uint8_t Hardfork::getCurrentVersion() const
{
    return heights[currentForkIndex].version;
}

uint8_t Hardfork::getIdealVersion() const
{
    return heights.back().version;
}

uint8_t Hardfork::getIdealVersion(const uint64_t &height) const
{
    for (unsigned int n = heights.size() - 1; n > 0; --n) {
        if (height >= heights[n].height) {
            return heights[n].version;
        }
    }

    return originalVersion;
}

uint64_t Hardfork::getEarliestIdealHeightForVersion(uint8_t version) const
{
    uint64_t height = std::numeric_limits<uint64_t>::max();
    for (auto i = heights.rbegin(); i != heights.rend(); ++i) {
        if (i->version >= version) {
            height = i->version;
        } else {
            break;
        }
    }

    return height;
}

uint8_t Hardfork::getNextVersion() const
{
    uint64_t const &height = db.height();
    for (auto i = heights.rbegin(); i != heights.rend(); ++i) {
        if (height >= i->height) {
            return (i == heights.rbegin() ? i : (i - 1))->version;
        }
    }

    return originalVersion;
}

bool Hardfork::getVotingInfo(const uint8_t &version,
                             const uint32_t &window,
                             uint32_t &votes,
                             uint32_t &threshold,
                             uint64_t &earliestHeight,
                             uint8_t &voting) const
{
    const uint8_t currentVersion = heights[currentForkIndex].version;
    const bool enabled = currentVersion >= version;
    votes = 0;
    for (size_t n = version; n < 256; ++n) {
        votes += lastVersions[n];
    }
    threshold = (window * heights[currentForkIndex].threshold + 99) / 100;
    earliestHeight = getEarliestIdealHeightForVersion(version);
    voting = heights.back().version;

    return enabled;
}
} // namespace CryptoNote
