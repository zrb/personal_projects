#include "iouring_service.hpp"

#include "iouring_impl_liburing.hpp"

namespace zsl::iouring
{

struct ring_t::impl_t : zsl::iouring::impl::liburing::impl_t
{
};

template < typename F, typename... Args >
void ring_t::prepare(ring_t::event_t & e, F && f, Args &&... args)
{
    impl_->prepare(e, std::forward < F >(f), std::forward < Args >(args)...);
}

}
