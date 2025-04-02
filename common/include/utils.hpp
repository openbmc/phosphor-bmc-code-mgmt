#pragma once
#include <unistd.h>

class UniqueFD
{
  public:
    explicit UniqueFD(int fd = -1) : fd_(fd) {}
    ~UniqueFD()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
        }
    }
    UniqueFD(const UniqueFD&) = delete;
    UniqueFD& operator=(const UniqueFD&) = delete;
    UniqueFD(UniqueFD&& other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }
    UniqueFD& operator=(UniqueFD&& other) noexcept
    {
        if (this != &other)
        {
            if (fd_ >= 0)
            {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    int get() const
    {
        return fd_;
    }
    bool is_valid() const
    {
        return fd_ >= 0;
    }

  private:
    int fd_;
};
