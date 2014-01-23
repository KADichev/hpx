//  Copyright (c)      2013 Thomas Heller
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_UNWRAPPED_HPP
#define HPX_UTIL_UNWRAPPED_HPP

#include <hpx/traits/is_future.hpp>
#include <hpx/traits/is_future_range.hpp>

#include <hpx/util/decay.hpp>
#include <hpx/util/tuple.hpp>

#include <boost/fusion/include/transform_view.hpp>
#include <boost/fusion/include/filter_view.hpp>
#include <boost/fusion/include/value_of.hpp>
#include <boost/fusion/include/size.hpp>
#include <boost/fusion/include/invoke.hpp>
#include <boost/fusion/include/as_vector.hpp>
#include <boost/fusion/include/fold.hpp>
#include <boost/ref.hpp>

namespace hpx { namespace util
{
    namespace detail
    {
        template <typename Future, typename IsFutureRange =
            typename traits::is_future_range<Future>::type>
        struct is_void_future_impl;

        template <typename Future>
        struct is_void_future_impl<Future, boost::mpl::true_>
        {
            typedef
                boost::mpl::bool_<
                    boost::is_void<
                        typename hpx::lcos::detail::future_traits<
                            typename hpx::util::decay<
                                Future
                            >::type::value_type
                        >::type
                    >::type::value
                >
                type;
        };

        template <typename Future>
        struct is_void_future_impl<Future, boost::mpl::false_>
        {
            typedef
                boost::mpl::bool_<
                    boost::is_void<
                        typename hpx::lcos::detail::future_traits<
                            Future
                        >::type
                    >::type::value
                >
                type;
        };

        template <typename Future>
        struct is_void_future
        {
            typedef typename is_void_future_impl<Future>::type type;
        };

        template <>
        struct is_void_future<boost::mpl::_> : boost::mpl::false_ {};

        template <
            typename Future
          , typename IsFutureRange = typename traits::is_future_range<Future>::type
          , typename IsVoidFuture = typename is_void_future<Future>::type
        >
        struct unwrapped_param_type_impl;

        template <
            typename Future
          , typename IsFutureRange
        >
        struct unwrapped_param_type_impl<Future, IsFutureRange, boost::mpl::true_>
        {
            typedef hpx::util::unused_type type;
        };

        template <typename Future>
        struct unwrapped_param_type_impl<Future, boost::mpl::true_, boost::mpl::false_>
        {
            typedef
                std::vector<
                    typename hpx::lcos::detail::future_traits<
                        typename util::decay<Future>::type::value_type
                    >::type
                >
                type;
        };

        template <typename Future>
        struct unwrapped_param_type_impl<Future, boost::mpl::false_, boost::mpl::false_>
          : hpx::lcos::detail::future_traits<Future>
        {};

        template <typename Future>
        struct unwrapped_param_type
        {
            typedef
                typename unwrapped_param_type_impl<
                    typename boost::unwrap_reference<Future>::type
                >::type
                type;
        };

        struct unwrapped_param_value
        {
            template <typename Sig>
            struct result;

            template <typename This, typename Tuple, typename Future>
            struct result<This(Tuple, Future)>
            {
                typedef
                    typename unwrapped_param_type<Future>::type
                    value_type;
                typedef
                    typename boost::mpl::eval_if<
                        boost::is_same<value_type, hpx::util::unused_type>
                      , boost::decay<Tuple>
                      , boost::fusion::result_of::push_back<
                            typename util::decay<Tuple>::type const
                          , value_type
                        >
                    >::type
                    type;
            };

            template <typename Tuple, typename Future>
            typename result<unwrapped_param_value(Tuple, Future)>::type
            operator()(Tuple t, Future & f) const
            {
                typedef
                    typename boost::unwrap_reference<
                        typename util::decay<Future>::type
                    >::type
                    future_type;

                return
                    invoke(
                        std::move(t)
                      , boost::unwrap_ref(f)
                      , typename traits::is_future_range<future_type>::type()
                      , typename is_void_future<future_type>::type()
                    );
            }

            template <typename Tuple, typename Future>
            typename result<unwrapped_param_value(Tuple, Future)>::type
            invoke(Tuple t, Future & f, boost::mpl::false_, boost::mpl::false_) const
            {
                HPX_ASSERT(f.is_ready());
                typename lcos::detail::future_traits<Future>::type
                    val(std::move(f.get()));
                f = Future();
                return
                    std::move(
                        boost::fusion::push_back(
                            std::move(t)
                          , std::move(val)
                        )
                    );
            }

            template <typename Tuple, typename Future>
            typename result<unwrapped_param_value(Tuple, Future)>::type
            invoke(Tuple t, Future & f, boost::mpl::false_, boost::mpl::true_) const
            {
                HPX_ASSERT(f.is_ready());
                f.get();
                f = Future();
                return std::move(t);
            }

            template <typename Tuple, typename Range>
            typename result<unwrapped_param_value(Tuple, Range)>::type
            invoke(Tuple t, Range & r, boost::mpl::true_, boost::mpl::false_) const
            {
                typename unwrapped_param_type<Range>::type res;

                res.reserve(r.size());

                BOOST_FOREACH(typename Range::value_type & f, r)
                {
                    HPX_ASSERT(f.is_ready());
                    res.push_back(std::move(f.get()));
                }
                r = Range();

                return
                    std::move(
                        boost::fusion::push_back(
                            std::move(t)
                          , std::move(res)
                        )
                    );
            }

            template <typename Tuple, typename Range>
            typename result<unwrapped_param_value(Tuple, Range)>::type
            invoke(Tuple t, Range & r, boost::mpl::true_, boost::mpl::true_) const
            {
                BOOST_FOREACH(typename Range::value_type const & f, r)
                {
                    HPX_ASSERT(f.is_ready());
                    f.get();
                }
                r = Range();

                return std::move(t);
            }
        };

        template <typename F>
        struct unwrapped_impl
        {
            F f_;

            template <typename Sig>
            struct result;

            template <typename This>
            struct result<This()>
            {
                typedef hpx::util::unused_type type;
            };

            hpx::util::unused_type operator()();
            hpx::util::unused_type operator()() const;

#define     HPX_UTIL_UNWRAP_IMPL_OPERATOR(Z, N, D)                              \
            template <typename This, BOOST_PP_ENUM_PARAMS(N, typename A)>       \
            struct result<This(BOOST_PP_ENUM_PARAMS(N, A))>                     \
            {                                                                   \
                typedef                                                         \
                    hpx::util::tuple<BOOST_PP_ENUM_PARAMS(N, A)>                \
                    params_type;                                                \
                typedef                                                         \
                    typename boost::fusion::result_of::invoke<                  \
                        F                                                       \
                      , typename boost::fusion::result_of::fold<                \
                            params_type                                         \
                          , hpx::util::tuple<>                                  \
                          , unwrapped_param_value                               \
                        >::type                                                 \
                    >::type                                                     \
                    type;                                                       \
            };                                                                  \
                                                                                \
            template <BOOST_PP_ENUM_PARAMS(N, typename A)>                      \
            typename result<unwrapped_impl(BOOST_PP_ENUM_PARAMS(N, A))>::type   \
            operator()(HPX_ENUM_FWD_ARGS(N, A, a))                              \
            {                                                                   \
                typedef                                                         \
                    hpx::util::tuple<BOOST_PP_ENUM_PARAMS(N, A)>                \
                    params_type;                                                \
                                                                                \
                params_type params(HPX_ENUM_FORWARD_ARGS(N, A, a));             \
                                                                                \
                return                                                          \
                    boost::fusion::invoke(                                      \
                        f_                                                      \
                      , boost::fusion::fold(                                    \
                            params                                              \
                          , hpx::util::tuple<>()                                \
                          , unwrapped_param_value()                             \
                        )                                                       \
                    );                                                          \
            }                                                                   \
            /**/
            BOOST_PP_REPEAT_FROM_TO(
                1
              , HPX_FUNCTION_ARGUMENT_LIMIT
              , HPX_UTIL_UNWRAP_IMPL_OPERATOR
              , _
            )
#undef      HPX_UTIL_UNWRAP_IMPL_OPERATOR
        };
    }

    template <typename F>
    detail::unwrapped_impl<typename util::decay<F>::type >
    unwrapped(F && f)
    {
        detail::unwrapped_impl<typename util::decay<F>::type >
            res = {std::forward<F>(f)};

        return std::move(res);
    }
}}

#endif