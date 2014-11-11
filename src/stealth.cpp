// Copyright (c) 2014 bushido
// Copyright (c) 2014 The Vertcoin developers
// Copyright (c) 2014 https://github.com/spesmilo/sx
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stealth.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/random_device.hpp>

hash_digest bitcoin_hash(const data_chunk& chunk)
{
    hash_digest first_hash;
    SHA256(chunk.data(), chunk.size(), first_hash.data());

    hash_digest second_hash;
    SHA256(first_hash.data(), first_hash.size(), second_hash.data());

    // The hash is in the reverse of the expected order.
    std::reverse(second_hash.begin(), second_hash.end());
    return second_hash;
}

uint32_t bitcoin_checksum(const data_chunk& chunk)
{
    hash_digest hash = bitcoin_hash(chunk);
    return from_little_endian<uint32_t>(hash.rbegin());
}

void append_checksum(data_chunk& data)
{
    uint32_t checksum = bitcoin_checksum(data);
    extend_data(data, to_little_endian(checksum));
}

std::string stealth_address::encoded() const
{
    data_chunk raw_addr;
    raw_addr.push_back(stealth_version_byte);
    raw_addr.push_back(options);
    extend_data(raw_addr, scan_pubkey);
    uint8_t number_spend_pubkeys = static_cast<uint8_t>(spend_pubkeys.size());
    raw_addr.push_back(number_spend_pubkeys);
    for (const ec_point& pubkey: spend_pubkeys)
        extend_data(raw_addr, pubkey);
    raw_addr.push_back(number_signatures);
    //assert_msg(prefix.number_bits == 0, "Not yet implemented!");
    raw_addr.push_back(0);
    append_checksum(raw_addr);
    return EncodeBase58(raw_addr);
}

bool verify_checksum(const data_chunk& data)
{
    data_chunk body(data.begin(), data.end() - 4);
    auto checksum = from_little_endian<uint32_t>(data.end() - 4);
    return bitcoin_checksum(body) == checksum;
}

bool stealth_address::set_encoded(const std::string& encoded_address)
{
    data_chunk raw_addr;
    DecodeBase58(encoded_address, raw_addr);
    if (!verify_checksum(raw_addr))
        return false;
    assert(raw_addr.size() >= 4);
    auto checksum_begin = raw_addr.end() - 4;
    // Delete checksum bytes.
    raw_addr.erase(checksum_begin, raw_addr.end());
    // https://wiki.unsystem.net/index.php/DarkWallet/Stealth#Address_format
    // [version] [options] [scan_key] [N] ... [Nsigs] [prefix_length] ...
    size_t estimated_data_size = 1 + 1 + 33 + 1 + 1 + 1;
    assert(raw_addr.size() >= estimated_data_size);
    auto iter = raw_addr.begin();
    uint8_t version = *iter;
    if (version != stealth_version_byte)
        return false;
    ++iter;
    options = *iter;
    ++iter;
    auto scan_key_begin = iter;
    iter += 33;
    scan_pubkey = data_chunk(scan_key_begin, iter);
    uint8_t number_spend_pubkeys = *iter;
    ++iter;
    estimated_data_size += number_spend_pubkeys * 33;
    assert(raw_addr.size() >= estimated_data_size);
    for (size_t i = 0; i < number_spend_pubkeys; ++i)
    {
        auto spend_key_begin = iter;
        iter += 33;
        spend_pubkeys.emplace_back(data_chunk(spend_key_begin, iter));
    }
    number_signatures = *iter;
    ++iter;
    prefix.number_bits = *iter;
    ++iter;
    size_t number_bitfield_bytes = 0;
    if (prefix.number_bits > 0)
        number_bitfield_bytes = prefix.number_bits / 8 + 1;
    estimated_data_size += number_bitfield_bytes;
    assert(raw_addr.size() >= estimated_data_size);
    // Unimplemented currently!
    assert(number_bitfield_bytes == 0);
    return true;
}

ec_secret generate_random_secret()
{
    using namespace boost::random;
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<uint8_t> dist(0, std::numeric_limits<uint8_t>::max());
    ec_secret secret;
    for (uint8_t& byte: secret)
        byte = dist(generator);
    return secret;
}

bool ec_multiply(ec_point& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ec_pubkey_tweak_mul(a.data(), a.size(), b.data());
}

hash_digest sha256_hash(const data_chunk& chunk)
{
    hash_digest hash;
    SHA256(chunk.data(), chunk.size(), hash.data());
    return hash;
}

ec_secret shared_secret(const ec_secret& secret, ec_point point)
{
    // diffie hellman stage
    bool success = ec_multiply(point, secret);
    assert(success);

    // start the second stage
    return sha256_hash(point);
}

bool ec_tweak_add(ec_point& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ec_pubkey_tweak_add(a.data(), a.size(), b.data());
}

ec_point secret_to_public_key(const ec_secret& secret,
    bool compressed)
{
    init.init();
    size_t size = ec_uncompressed_size;
    if (compressed)
        size = ec_compressed_size;

    ec_point out(size);
    int out_size;
    if (!secp256k1_ec_pubkey_create(out.data(), &out_size, secret.data(),
            compressed))
        return ec_point();
    assert(size == static_cast<size_t>(out_size));
    return out;
}

ec_point initiate_stealth(
    const ec_secret& ephem_secret, const ec_point& scan_pubkey,
    const ec_point& spend_pubkey)
{
    ec_point final = spend_pubkey;

    // Generate shared secret
    ec_secret shared = shared_secret(ephem_secret, scan_pubkey);

    // Now generate address
    bool success = ec_tweak_add(final, shared);
    assert(success);
    return final;
}

short_hash bitcoin_short_hash(const data_chunk& chunk)
{
    hash_digest sha_hash;
    SHA256(chunk.data(), chunk.size(), sha_hash.data());

    short_hash ripemd_hash;
    RIPEMD160(sha_hash.data(), sha_hash.size(), ripemd_hash.data());

    return ripemd_hash;
}


void set_public_key(payment_address& address, const data_chunk& public_key)
{
    address.set(fTestNet ? CBitcoinAddress::PUBKEY_ADDRESS_TEST : CBitcoinAddress::PUBKEY_ADDRESS,
        bitcoin_short_hash(public_key));
}

payment_address::payment_address() : version_(invalid_version), hash_(null_short_hash)
{
}

payment_address::payment_address(uint8_t version, const short_hash& hash)
{
    payment_address();
    set(version, hash);
}

payment_address::payment_address(const std::string& encoded_address)
{
    payment_address();
    set_encoded(encoded_address);
}

void payment_address::set(uint8_t version, const short_hash& hash)
{
    version_ = version;
    hash_ = hash;
}

bool is_base58(const char c)
{
    auto last = std::end(base58_chars) - 1;
    // This works because the base58 characters happen to be in sorted order
    return std::binary_search(base58_chars, last, c);
}

bool is_base58(const std::string& text)
{
    return std::all_of(text.begin(), text.end(),
        [](const char c){ return is_base58(c); });
}

bool payment_address::set_encoded(const std::string& encoded_address)
{
    if (!is_base58(encoded_address))
        return false;
    data_chunk decoded_address;
    DecodeBase58(encoded_address, decoded_address);
    // version + 20 bytes short hash + 4 bytes checksum
    if (decoded_address.size() != 25)
        return false;
    if (!verify_checksum(decoded_address))
        return false;

    version_ = decoded_address[0];
    std::copy_n(decoded_address.begin() + 1, hash_.size(), hash_.begin());
    return true;
}

std::string payment_address::encoded() const
{
    data_chunk unencoded_address;
    unencoded_address.reserve(25);
    // Type, Hash, Checksum doth make thy address
    unencoded_address.push_back(version_);
    extend_data(unencoded_address, hash_);
    append_checksum(unencoded_address);
    assert(unencoded_address.size() == 25);
    return EncodeBase58(unencoded_address);
}

ec_point uncover_stealth(
    const ec_point& ephem_pubkey, const ec_secret& scan_secret,
    const ec_point& spend_pubkey)
{
    ec_point final = spend_pubkey;
    ec_secret shared = shared_secret(scan_secret, ephem_pubkey);
    bool success = ec_tweak_add(final, shared);
    assert(success);
    return final;
}

bool ec_add(ec_secret& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ec_privkey_tweak_add(a.data(), b.data());
}

ec_secret uncover_stealth_secret(
    const ec_point& ephem_pubkey, const ec_secret& scan_secret,
    const ec_secret& spend_secret)
{
    ec_secret final = spend_secret;
    ec_secret shared = shared_secret(scan_secret, ephem_pubkey);
    bool success = ec_add(final, shared);
    assert(success);
    return final;
}

std::string secret_to_wif(const ec_secret& secret, bool compressed)
{
    data_chunk data;
    data.reserve(1 + hash_size + 1 + 4);

    data.push_back(fTestNet ? CBitcoinSecret::PRIVKEY_ADDRESS_TEST : CBitcoinSecret::PRIVKEY_ADDRESS);
    extend_data(data, secret);
    if (compressed)
        data.push_back(0x01);

    append_checksum(data);
    return EncodeBase58(data);
}

data_chunk decode_hex(std::string hex)
{
    // Trim the fat.
    boost::algorithm::trim(hex);
    data_chunk result(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        assert(hex.size() - i >= 2);
        auto byte_begin = hex.begin() + i;
        auto byte_end = hex.begin() + i + 2;
        // Perform conversion.
        int val = -1;
        std::stringstream converter;
        converter << std::hex << std::string(byte_begin, byte_end);
        converter >> val;
        if (val == -1)
            return data_chunk();
        assert(val <= 0xff);
        // Set byte.
        result[i / 2] = val;
    }
    return result;
}



