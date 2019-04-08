// Copyright (c) 2014-2017, The Monero Project
// Copyright (c) 2017-2018, Karbo developers
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

/*!
 * \file electrum-words.cpp
 *
 * \brief Mnemonic seed generation and wallet restoration from them.
 *
 * This file and its header file are for translating Electrum-style word lists
 * into their equivalent byte representations for cross-compatibility with
 * that method of "backing up" one's wallet keys.
 */

#include <string>
#include <cassert>
#include <map>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <stdexcept>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>
#include "crypto/crypto.h"  // for declaration of Crypto::SecretKey
#include "Common/Lazy.h"
#include "Mnemonics/electrum-words.h"

#include "chinese_simplified.h"
#include "english.h"
#include "dutch.h"
#include "french.h"
#include "italian.h"
#include "german.h"
#include "spanish.h"
#include "portuguese.h"
#include "japanese.h"
#include "polish.h"
#include "russian.h"
#include "ukrainian.h"
#include "language_base.h"

namespace Language {

const std::string English   ::c_name = "English";
const std::string Ukrainian ::c_name = "українська мова";
const std::string Polish    ::c_name = "język polski";
const std::string German    ::c_name = "Deutsch";
const std::string French    ::c_name = "Français";
const std::string Spanish   ::c_name = "Español";
const std::string Italian   ::c_name = "Italiano";
const std::string Portuguese::c_name = "Português";
const std::string Dutch     ::c_name = "Nederlands";
const std::string Japanese  ::c_name = "日本語";
const std::string ChineseSim::c_name = "简体中文 (中国)";
const std::string Russian   ::c_name = "русский язык";

typedef std::unordered_map<std::string, Lazy<std::shared_ptr<Base>>> LanguageMap;
const static LanguageMap c_languageMap =
{
	{ English   ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<English>();    }) },
	{ Ukrainian ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Ukrainian>();  }) },
	{ Polish    ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Polish>();     }) },
	{ German    ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<German>();     }) },
	{ French    ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<French>();     }) },
	{ Spanish   ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Spanish>();    }) },
	{ Italian   ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Italian>();    }) },
	{ Portuguese::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Portuguese>(); }) },
	{ Dutch     ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Dutch>();      }) },
	{ Japanese  ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Japanese>();   }) },
	{ ChineseSim::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<ChineseSim>(); }) },
	{ Russian   ::c_name, Lazy<std::shared_ptr<Base>>([](){ return std::make_shared<Russian>();    }) },
};

} //Language

namespace {

uint32_t create_checksum_index(const std::vector<std::string> &word_list, uint32_t unique_prefix_length);
bool checksum_test(std::vector<std::string> seed, uint32_t unique_prefix_length);

/*!
* \brief Finds the word list that contains the seed words and puts the indices
*        where matches occured in matched_indices.
* \param  seed            List of words to match.
* \param  has_checksum    The seed has a checksum word (maybe not checked).
* \param  matched_indices The indices where the seed words were found are added to this.
* \param  language        Language instance pointer to write to after it is found.
* \return                 true if all the words were present in some language false if not.
*/
bool find_seed_language(const std::vector<std::string> &seed, bool has_checksum,
	std::vector<uint32_t> &matched_indices, Language::Base **language)
{
	Language::Base *fallback = NULL;

	// Iterate through all the languages and find a match
	for (auto it1 = Language::c_languageMap.cbegin(); it1 != Language::c_languageMap.cend(); ++it1)
	{
		std::shared_ptr<Language::Base>& lang = it1->second;

		const std::unordered_map<std::string, uint32_t> &word_map         = lang->get_word_map();
		const std::unordered_map<std::string, uint32_t> &trimmed_word_map = lang->get_trimmed_word_map();
		// To iterate through seed words
		std::vector<std::string>::const_iterator it2;
		bool full_match = true;

		std::string trimmed_word;
		// Iterate through all the words and see if they're all present
		for (it2 = seed.cbegin(); it2 != seed.cend(); ++it2)
		{
			if (has_checksum)
			{
				trimmed_word = Language::utf8prefix(*it2, lang->get_unique_prefix_length());
				// Use the trimmed words and map
				if (trimmed_word_map.count(trimmed_word) == 0)
				{
					full_match = false;
					break;
				}
				matched_indices.push_back(trimmed_word_map.at(trimmed_word));
			}
			else
			{
				if (word_map.count(*it2) == 0)
				{
					full_match = false;
					break;
				}
				matched_indices.push_back(word_map.at(*it2));
			}
		}

		if (full_match)
		{
			// if we were using prefix only, and we have a checksum, check it now
			// to avoid false positives due to prefix set being too common
			if (has_checksum &&
				!checksum_test(seed, lang->get_unique_prefix_length()))
			{
				fallback = lang.get();
				full_match = false;
			}
		}

		if (full_match)
		{
			*language = lang.get();
			return true;
		}
		// Some didn't match. Clear the index array.
		matched_indices.clear();
	}

	// if we get there, we've not found a good match, but we might have a fallback,
	// if we detected a match which did not fit the checksum, which might be a badly
	// typed/transcribed seed in the right language
	if (fallback)
	{
		*language = fallback;
		return true;
	}

	return false;
}

/*!
* \brief Creates a checksum index in the word list array on the list of words.
* \param  word_list            Vector of words
* \param unique_prefix_length  the prefix length of each word to use for checksum
* \return                      Checksum index
*/
uint32_t create_checksum_index(const std::vector<std::string> &word_list, uint32_t unique_prefix_length)
{
	std::string trimmed_words = "";

	for (auto it = word_list.cbegin(); it != word_list.cend(); ++it)
	{
		if (it->length() > unique_prefix_length)
			trimmed_words += Language::utf8prefix(*it, unique_prefix_length);
		else
			trimmed_words += *it;
	}
	boost::crc_32_type result;
	result.process_bytes(trimmed_words.data(), trimmed_words.length());
	return result.checksum() % Crypto::ElectrumWords::seed_length;
}

/*!
* \brief Does the checksum test on the seed passed.
* \param seed                  Vector of seed words
* \param unique_prefix_length  the prefix length of each word to use for checksum
* \return                      True if the test passed false if not.
*/
bool checksum_test(std::vector<std::string> seed, uint32_t unique_prefix_length)
{
	// The last word is the checksum.
	std::string last_word = seed.back();
	seed.pop_back();

	std::string checksum = seed[create_checksum_index(seed, unique_prefix_length)];

	std::string trimmed_checksum = checksum.length() > unique_prefix_length ?
		Language::utf8prefix(checksum, unique_prefix_length) : checksum;
	std::string trimmed_last_word = last_word.length() > unique_prefix_length ?
		Language::utf8prefix(last_word, unique_prefix_length) : last_word;
	return trimmed_checksum == trimmed_last_word;
}

} //(unnamed)

/*!
 * \namespace Crypto
 *
 * \brief Crypto namespace.
 */
namespace Crypto {
namespace ElectrumWords {

/*!
* \brief Converts seed words to bytes (secret key).
* \param  words           String containing the words separated by spaces.
* \param  dst             To put the secret key restored from the words.
* \param  language_name   Language of the seed as found gets written here.
* \return                 false if not a multiple of 3 words, or if word is not in the words list
*/
bool words_to_bytes(std::string words, Crypto::SecretKey& dst, std::string &language_name)
{
	std::vector<std::string> seed;

	boost::algorithm::trim(words);
	boost::split(seed, words, boost::is_any_of(" "), boost::token_compress_on);

	// error on non-compliant word list
	if (seed.size() != seed_length/2 && seed.size() != seed_length && seed.size() != seed_length + 1)
		return false;

	// If it is seed with a checksum.
	bool has_checksum = seed.size() == (seed_length + 1);

	std::vector<uint32_t> matched_indices;
	Language::Base *language;
	if (!find_seed_language(seed, has_checksum, matched_indices, &language))
		return false;

	language_name = language->get_language_name();
	uint32_t word_list_length = static_cast<uint32_t>(language->get_word_list().size());

	if (has_checksum)
	{
		if (!checksum_test(seed, language->get_unique_prefix_length()))
			return false; // Checksum fail
		seed.pop_back();
	}

	for (unsigned int i=0; i < seed.size() / 3; i++)
	{
		uint32_t val;
		uint32_t w1, w2, w3;
		w1 = matched_indices[i*3];
		w2 = matched_indices[i*3 + 1];
		w3 = matched_indices[i*3 + 2];

		val = w1 + word_list_length * (((word_list_length - w1) + w2) % word_list_length) +
			word_list_length * word_list_length * (((word_list_length - w2) + w3) % word_list_length);

		if (!(val % word_list_length == w1)) return false;

		memcpy(dst.data + i * 4, &val, 4);  // copy 4 bytes to position
	}

	std::string wlist_copy = words;
	if (seed.size() == seed_length/2)
	{
		memcpy(dst.data+16, dst.data, 16);  // if electrum 12-word seed, duplicate
		wlist_copy += ' ';
		wlist_copy += words;
	}

	return true;
}

/*!
* \brief Converts bytes (secret key) to seed words.
* \param  src           Secret key
* \param  words         Space delimited concatenated words get written here.
* \param  language_name Seed language name
* \return               true if successful false if not. Unsuccessful if wrong key size.
*/
bool bytes_to_words(const Crypto::SecretKey& src, std::string& words, const std::string &language_name)
{
	if (sizeof(src.data) % 4 != 0 || sizeof(src.data) == 0) return false;

	auto itLanguage = Language::c_languageMap.find(language_name);
    if (itLanguage == Language::c_languageMap.cend())
		return false;
	std::shared_ptr<Language::Base>& language = itLanguage->second;

	const std::vector<std::string> &word_list = language->get_word_list();
	// To store the words for random access to add the checksum word later.
	std::vector<std::string> words_store;

    uint32_t word_list_length = static_cast<uint32_t>(word_list.size());
    // 8 bytes -> 3 words.  8 digits base 16 -> 3 digits base 1626
    for (unsigned int i=0; i < sizeof(src.data)/4; i++, words += ' ')
    {
		uint32_t w1, w2, w3;
		uint32_t val;

		memcpy(&val, (src.data) + (i * 4), 4);

		w1 = val % word_list_length;
		w2 = ((val / word_list_length) + w1) % word_list_length;
		w3 = (((val / word_list_length) / word_list_length) + w2) % word_list_length;

		words += word_list[w1];
		words += ' ';
		words += word_list[w2];
		words += ' ';
		words += word_list[w3];

		words_store.push_back(word_list[w1]);
		words_store.push_back(word_list[w2]);
		words_store.push_back(word_list[w3]);
	}

	words.pop_back();
	words += (' ' + words_store[create_checksum_index(words_store, language->get_unique_prefix_length())]);
	return true;
}

/*!
* \brief Gets a list of seed languages that are supported.
* \param languages The vector is set to the list of languages.
*/
void get_language_list(std::vector<std::string> &languages)
{
    languages.clear();
	auto itBegin = Language::c_languageMap.cbegin();
	auto itEnd   = Language::c_languageMap.cend();
	std::transform(itBegin, itEnd, std::back_inserter(languages),
		[](const Language::LanguageMap::value_type& pair){ return pair.first; });
}

/*!
* \brief Tells if the seed passed is an old style seed or not.
* \param  seed The seed to check (a space delimited concatenated word list)
* \return      true if the seed passed is a old style seed false if not.
*/
bool get_is_old_style_seed(std::string seed)
{
    std::vector<std::string> word_list;
    boost::algorithm::trim(seed);
    boost::split(word_list, seed, boost::is_any_of(" "), boost::token_compress_on);
    return word_list.size() != (seed_length + 1);
}

} //ElectrumWords
} //Crypto
