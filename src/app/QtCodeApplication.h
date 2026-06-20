#pragma once

#include <QApplication>
#include <memory>

namespace qtcode::core {
class ApplicationController;
} // namespace qtcode::core

class QtCodeApplication
{
public:
    QtCodeApplication(int &argc, char **argv);
    ~QtCodeApplication();

    QtCodeApplication(const QtCodeApplication &) = delete;
    QtCodeApplication &operator=(const QtCodeApplication &) = delete;

    [[nodiscard]] int run();
    [[nodiscard]] QApplication &application() noexcept;

private:
    void configureMetadata();

    std::unique_ptr<QApplication> m_application;
    std::unique_ptr<qtcode::core::ApplicationController> m_controller;
};
