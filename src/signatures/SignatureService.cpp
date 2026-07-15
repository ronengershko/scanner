#include "signatures/SignatureService.h"
#include <iostream>
#include <stdexcept>

static std::vector<std::byte> toBytes(const std::string& s) {
    std::vector<std::byte> result;
    result.reserve(s.size());
    for (unsigned char c : s)
        result.push_back(static_cast<std::byte>(c));
    return result;
}

SignatureService::SignatureService(SignatureRepository& repo, Logger& logger)
    : m_repo(repo), m_logger(logger) {}

void SignatureService::list() const {
    auto records = m_repo.loadAll();
    if (records.empty()) {
        std::cout << "No signatures configured.\n";
        return;
    }
    std::cout << "ID  Value\n";
    for (const auto& r : records)
        std::cout << r.id << "   " << r.value << "\n";
}

void SignatureService::add(const std::string& value) {
    if (value.empty())
        throw std::invalid_argument("Signature value cannot be empty.");

    int64_t id = m_repo.add(value);
    m_repo.incrementVersion();
    m_logger.info("Signature added id=" + std::to_string(id) + " value=" + value);
    std::cout << "Signature added with id " << id << ".\n";
}

void SignatureService::remove(int64_t id) {
    m_repo.remove(id);
    m_repo.incrementVersion();
    m_logger.info("Signature removed id=" + std::to_string(id));
    std::cout << "Signature " << id << " removed.\n";
}

int64_t SignatureService::getVersion() const {
    return m_repo.getVersion();
}

std::vector<Signature> SignatureService::loadForScanning() const {
    auto records = m_repo.loadAll();
    std::vector<Signature> result;
    result.reserve(records.size());
    for (const auto& r : records)
        result.push_back({r.id, toBytes(r.value)});
    return result;
}
