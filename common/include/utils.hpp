#include <unistd.h>

class UniqueFD
{
  public:
    /**
     * @brief Construct a UniqueFD with an optional file descriptor.
     *
     * @param[in] fd The file descriptor to wrap. Defaults to -1 (invalid).
     */
    explicit UniqueFD(int fd = -1) : fd_(fd) {}

    /**
     * @brief Destructor. Closes the file descriptor if valid.
     */
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

    /**
     * @brief Retrieve the underlying file descriptor.
     *
     * @return The file descriptor integer.
     */
    int get() const
    {
        return fd_;
    }

    /**
     * @brief Check if the file descriptor is valid.
     *
     * @return True if valid, false otherwise.
     */
    bool is_valid() const
    {
        return fd_ >= 0;
    }

  private:
    int fd_;
};