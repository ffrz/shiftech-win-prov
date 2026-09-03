#pragma once

#include "DriverProvider.h"

namespace shiftech::core::drivers {

class MockDriverProvider : public DriverProvider {
public:
    explicit MockDriverProvider(const std::string& indexFilePath);
    
    DriverSearchResult search(const hardware::Device& device,
                              const TargetSystem& target) override;
                              
    std::string name() const override { return "mock"; }

private:
    std::string indexPath;
};

} // namespace shiftech::core::drivers
