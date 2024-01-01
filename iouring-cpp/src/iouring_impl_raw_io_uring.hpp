#pragma once

#error "Not yet implemented"

#include "iouring_service.hpp"

namespace zsl::iouring::impl::raw_io_uring
{

struct impl_t
{
    struct data_t
    {
        enum class fd_t : int32_t;
        constexpr static fd_t const invalid_fd{-1};

        params_t params_{
            .sq_entries = 64,
            .cq_entries = 1024,
            .flags = IORING_SETUP_IOPOLL | IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF | IORING_SETUP_CQSIZE | IORING_SETUP_CLAMP | IORING_SETUP_SUBMIT_ALL | IORING_SETUP_COOP_TASKRUN,
            .sq_thread_cpu = 0b111000,
            .sq_thread_idle = 0,
            .features = 0,
            .wq_fd = 0,
            .resv = {},
            .sq_off = {},
            .cq_off = {},
        };
        
        fd_t fd_{invalid_fd};

        constexpr auto is_valid() const noexcept
        {
            return fd_ != invalid_fd;
        }


        data_t() : fd_{io_uring_setup(1024, &params_)}
        {
            if (!is_valid())
                throw std::runtime_error("Unable to create io_uring ring");
        }

        ~data_t()
        {
            if (is_valid())
                close(std::to_underlying(fd_));
        }
    };

    data_t data_{};
};

}

