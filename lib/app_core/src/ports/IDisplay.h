#pragma once

#include "app/AppViewModel.h"

class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual void render(const AppViewModel& model) = 0;
};
