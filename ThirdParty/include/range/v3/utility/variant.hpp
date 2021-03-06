/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_UTILITY_VARIANT_HPP
#define RANGES_V3_UTILITY_VARIANT_HPP

#include <new>
#include <utility>
#include <type_traits>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/utility/concepts.hpp>
#include <range/v3/utility/functional.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// \cond
        namespace detail
        {
            using add_ref_t =
                meta::quote_trait<std::add_lvalue_reference>;

            using add_cref_t =
                meta::compose<meta::quote_trait<std::add_lvalue_reference>, meta::quote_trait<std::add_const>>;
        }
        /// \endcond

        template<std::size_t I>
        struct emplaced_index_t;

        template<typename...Ts>
        struct variant;

        template<std::size_t N, typename Var>
        struct variant_element;

        template<std::size_t N, typename Var>
        using variant_element_t =
            meta::_t<variant_element<N, Var>>;

        template<std::size_t I>
        struct emplaced_index_t
          : meta::size_t<I>
        {};

        /// \cond
    #if !RANGES_CXX_VARIABLE_TEMPLATES
        template<std::size_t I>
        inline emplaced_index_t<I> emplaced_index()
        {
            return {};
        }
        template<std::size_t I>
        using _emplaced_index_t_ = emplaced_index_t<I>(&)();
    #define RANGES_EMPLACED_INDEX_T(I) _emplaced_index_t_<I>
    #else
        /// \endcond
        namespace
        {
            template<std::size_t I>
#ifdef RANGES_WORKAROUND_MSVC_AUTO_VARIABLE_TEMPLATE
            constexpr const emplaced_index_t<I> &emplaced_index = static_const<emplaced_index_t<I>>::value;
#else
            constexpr auto &emplaced_index = static_const<emplaced_index_t<I>>::value;
#endif
        }
        /// \cond
    #define RANGES_EMPLACED_INDEX_T(I) emplaced_index_t<I>
    #endif
        /// \endcond

        /// \cond
        namespace detail
        {
            template<typename Fun, typename T, std::size_t N,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                CONCEPT_REQUIRES_(Function<Fun, T &, meta::size_t<N>>::value)>
#else
                CONCEPT_REQUIRES_(Function<Fun, T &, meta::size_t<N>>())>
#endif
            void apply_if(Fun fun, T &t, meta::size_t<N> u)
            {
                fun(t, u);
            }

            template<class T>
            [[noreturn]] inline void apply_if(T, any, any)
            {
                RANGES_ENSURE(false);
            }

            template<typename...List>
            union variant_data;

            template<>
            union variant_data<>
            {
                template<typename That,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Same<variant_data, uncvref_t<That>>::value)>
#else
                    CONCEPT_REQUIRES_(Same<variant_data, uncvref_t<That>>())>
#endif
                [[noreturn]] void move_copy_construct(std::size_t, That &&) const
                {
                    RANGES_ENSURE(false);
                }
                [[noreturn]] bool equal(std::size_t, variant_data const &) const
                {
                    RANGES_ENSURE(false);
                }
                template<typename Fun, std::size_t N = 0>
                [[noreturn]] void apply(std::size_t, Fun &&, meta::size_t<N> = meta::size_t<N>{}) const
                {
                    RANGES_ENSURE(false);
                }
            };

            template<typename T, typename ...Ts>
            union variant_data<T, Ts...>
            {
            private:
                template<typename...Us>
                friend union variant_data;
                using head_t = decay_t<meta::if_<std::is_reference<T>, ref_t<T &>, T>>;
                using tail_t = variant_data<Ts...>;

                head_t head;
                tail_t tail;

                template<typename This, typename Fun, std::size_t N>
                static void apply_(This &this_, std::size_t n, Fun fun, meta::size_t<N> u)
                {
                    if(0 == n)
                        detail::apply_if(detail::move(fun), this_.head, u);
                    else
                        this_.tail.apply(n - 1, detail::move(fun), meta::size_t<N + 1>{});
                }
            public:
                variant_data() {}
                template<typename ...Args,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Constructible<head_t, Args...>::value)>
#else
                    CONCEPT_REQUIRES_(Constructible<head_t, Args...>())>
#endif
                constexpr variant_data(meta::size_t<0>, Args && ...args)
                  : head(detail::forward<Args>(args)...)
                {}
                template<std::size_t N, typename ...Args,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(0 != N && Constructible<tail_t, meta::size_t<N - 1>, Args...>::value)>
#else
                    CONCEPT_REQUIRES_(0 != N && Constructible<tail_t, meta::size_t<N - 1>, Args...>())>
#endif
                constexpr variant_data(meta::size_t<N>, Args && ...args)
                  : tail{meta::size_t<N - 1>{}, detail::forward<Args>(args)...}
                {}
                ~variant_data()
                {}
                template<typename That,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Same<variant_data, decay_t<That>>::value)>
#else
                    CONCEPT_REQUIRES_(Same<variant_data, decay_t<That>>())>
#endif
                void move_copy_construct(std::size_t n, That &&that)
                {
                    if(n == 0)
                        ::new(static_cast<void *>(&head)) head_t(std::forward<That>(that).head);
                    else
                        tail.move_copy_construct(n - 1, std::forward<That>(that).tail);
                }
                template<typename U, typename...Us>
                bool equal(std::size_t n, variant_data<U, Us...> const &that) const
                {
                    if(n == 0)
                        return head == that.head;
                    else
                        return tail.equal(n - 1, that.tail);
                }
                template<typename Fun, std::size_t N = 0>
                void apply(std::size_t n, Fun fun, meta::size_t<N> u = {})
                {
                    variant_data::apply_(*this, n, detail::move(fun), u);
                }
                template<typename Fun, std::size_t N = 0>
                void apply(std::size_t n, Fun fun, meta::size_t<N> u = {}) const
                {
                    variant_data::apply_(*this, n, detail::move(fun), u);
                }
            };

            struct empty_variant_tag { };

            struct variant_core_access
            {
                template<typename...Ts>
                static variant_data<Ts...> &data(variant<Ts...> &var)
                {
                    return var.data_;
                }

                template<typename...Ts>
                static variant_data<Ts...> const &data(variant<Ts...> const &var)
                {
                    return var.data_;
                }

                template<typename...Ts>
                static variant_data<Ts...> &&data(variant<Ts...> &&var)
                {
                    return std::move(var).data_;
                }

                template<typename...Ts>
                static variant<Ts...> make_empty(meta::id<variant<Ts...>>)
                {
                    return variant<Ts...>{empty_variant_tag{}};
                }
            };

            struct delete_fun
            {
                template<typename T>
                void operator()(T const &t) const
                {
                    t.~T();
                }
            };

            struct assert_otherwise
            {
#ifdef RANGES_WORKAROUND_MSVC_OPERATOR_ACCESS
                [[noreturn]] void operator()(any) const
                {
                    RANGES_ENSURE(false);
                }
#else
                [[noreturn]] static void assert_false(any) { RANGES_ENSURE(false); }
                using fun_t = void(*)(any);
                operator fun_t() const
                {
                    return &assert_false;
                }
#endif
            };

            // Is there a less dangerous way?
            template<typename...Ts>
            struct construct_fun : assert_otherwise
            {
            private:
                std::tuple<Ts...> t_;

                template<typename U, std::size_t ...Is,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Constructible<U, Ts...>::value)>
#else
                    CONCEPT_REQUIRES_(Constructible<U, Ts...>())>
#endif
                void construct(U &u, meta::index_sequence<Is...>)
                {
                    ::new((void*)std::addressof(u)) U(std::get<Is>(std::move(t_))...);
                }
            public:
                construct_fun(Ts ...ts)
                  : t_{std::forward<Ts>(ts)...}
                {}
                template<typename U,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                    CONCEPT_REQUIRES_(Constructible<U, Ts...>::value)>
#else
                    CONCEPT_REQUIRES_(Constructible<U, Ts...>())>
#endif
                void operator()(U &u)
                {
                    this->construct(u, meta::make_index_sequence<sizeof...(Ts)>{});
                }
#ifdef RANGES_WORKAROUND_MSVC_OPERATOR_ACCESS
                using assert_otherwise::operator();
#endif
            };

            template<typename T>
            struct get_fun : assert_otherwise
            {
            private:
                T **t_;
            public:
                get_fun(T *&t)
                  : t_(&t)
                {}
                void operator()(T &t) const
                {
                    *t_ = std::addressof(t);
                }
#ifdef RANGES_WORKAROUND_MSVC_OPERATOR_ACCESS
                using assert_otherwise::operator();
#endif
            };

            template<typename Fun, typename Var = std::nullptr_t>
            struct apply_visitor
              : private function_type<Fun>
            {
            private:
                using BaseFun = function_type<Fun>;
                Var *var_;

                BaseFun &fn() { return *this; }
                BaseFun const &fn() const { return *this; }

                template<typename T, std::size_t N>
                void apply_(T &&t, meta::size_t<N> n, std::true_type) const
                {
                    fn()(std::forward<T>(t), n);
                    var_->template emplace<N>(nullptr);
                }
                template<typename T, std::size_t N>
                void apply_(T &&t, meta::size_t<N> n, std::false_type) const
                {
                    var_->template emplace<N>(fn()(std::forward<T>(t), n));
                }
            public:
                apply_visitor(Fun fun, Var &var)
                  : BaseFun(as_function(detail::move(fun)))
                  , var_(std::addressof(var))
                {}
                template<typename T, std::size_t N,
                         typename result_t = result_of_t<Fun(T &&, meta::size_t<N>)>>
                auto operator()(T &&t, meta::size_t<N> u) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    this->apply_(std::forward<T>(t), u, std::is_void<result_t>{})
                )
            };

            template<typename Fun>
            struct apply_visitor<Fun, std::nullptr_t>
              : private function_type<Fun>
            {
            private:
                using BaseFun = function_type<Fun>;

                BaseFun &fn() { return *this; }
                BaseFun const &fn() const { return *this; }
            public:
                apply_visitor(Fun fun)
                  : BaseFun(as_function(detail::move(fun)))
                {}
                template<typename T, std::size_t N>
                auto operator()(T &&t, meta::size_t<N> n) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    this->fn()(std::forward<T>(t), n), void()
                )
            };

            template<typename Fun>
            struct ignore2nd
              : private function_type<Fun>
            {
            private:
                using BaseFun = function_type<Fun>;

                BaseFun &fn() { return *this; }
                BaseFun const &fn() const { return *this; }
            public:
                ignore2nd(Fun &&fun)
                  : BaseFun(as_function(detail::move(fun)))
                {}
                template<typename T, typename U>
                auto operator()(T &&t, U &&) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    this->fn()(std::forward<T>(t))
                )
            };

            template<typename Fun>
            apply_visitor<ignore2nd<Fun>> make_unary_visitor(Fun fun)
            {
                return {detail::move(fun)};
            }
            template<typename Fun, typename Var>
            apply_visitor<ignore2nd<Fun>, Var> make_unary_visitor(Fun fun, Var &var)
            {
                return {detail::move(fun), var};
            }
            template<typename Fun>
            apply_visitor<Fun> make_binary_visitor(Fun fun)
            {
                return {detail::move(fun)};
            }
            template<typename Fun, typename Var>
            apply_visitor<Fun, Var> make_binary_visitor(Fun fun, Var &var)
            {
                return {detail::move(fun), var};
            }

            template<typename To, typename From>
            struct unique_visitor;

            template<typename ...To, typename ...From>
            struct unique_visitor<variant<To...>, variant<From...>>
            {
            private:
                variant<To...> *var_;
            public:
                unique_visitor(variant<To...> &var)
                  : var_(&var)
                {}
                template<typename T, std::size_t N>
                void operator()(T &&t, meta::size_t<N>) const
                {
                    using E = meta::at_c<meta::list<From...>, N>;
                    using F = meta::find<meta::list<To...>, E>;
                    static constexpr std::size_t M = sizeof...(To) - F::size();
                    var_->template emplace<M>(std::forward<T>(t));
                }
            };

            template<typename Fun, typename Types>
            using variant_result_t =
                meta::apply_list<
                    meta::quote<variant>,
                    meta::replace<
                        meta::transform<
                            Types,
                            meta::bind_front<meta::quote<concepts::Function::result_t>, Fun>>,
                        void,
                        std::nullptr_t>>;

            template<typename Fun, typename Types>
            using variant_result_i_t =
                meta::apply_list<
                    meta::quote<variant>,
                    meta::replace<
                        meta::transform<
                            Types,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                            meta::as_list<meta::make_index_sequence<meta::size<Types>::value>>,
#else
                            meta::as_list<meta::make_index_sequence<Types::size()>>,
#endif
                            meta::bind_front<meta::quote<concepts::Function::result_t>, Fun>>,
                        void,
                        std::nullptr_t>>;

            template<typename Fun>
            struct unwrap_ref_fun
              : private function_type<Fun>
            {
            private:
                using BaseFun = function_type<Fun>;

                BaseFun &fn() { return *this; }
                BaseFun const &fn() const { return *this; }
            public:
                unwrap_ref_fun(Fun fun)
                  : BaseFun(as_function(detail::move(fun)))
                {}
                template<typename T>
                auto operator()(T &&t) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    this->fn()(unwrap_reference(std::forward<T>(t)))
                )
                template<typename T, std::size_t N>
                auto operator()(T &&t, meta::size_t<N> n) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    this->fn()(unwrap_reference(std::forward<T>(t)), n)
                )
            };
        }
        /// \endcond

        /// \addtogroup group-utility
        /// @{
        template<typename ...Ts>
        struct variant
        {
        private:
            friend struct detail::variant_core_access;
            using types_t = meta::list<Ts...>;
            using data_t = detail::variant_data<Ts...>;
            std::size_t which_;
            data_t data_;

            void clear_()
            {
                if(is_valid())
                {
                    data_.apply(which_, detail::make_unary_visitor(detail::delete_fun{}));
                    which_ = (std::size_t)-1;
                }
            }

            template<typename That,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                CONCEPT_REQUIRES_(Same<variant, detail::decay_t<That>>::value)>
#else
                CONCEPT_REQUIRES_(Same<variant, detail::decay_t<That>>())>
#endif
            void assign_(That &&that)
            {
                if(that.is_valid())
                {
                    data_.move_copy_construct(that.which_, std::forward<That>(that).data_);
                    which_ = that.which_;
                }
            }

            constexpr variant(detail::empty_variant_tag)
              : which_((std::size_t)-1)
            {}

        public:
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
            CONCEPT_REQUIRES(Constructible<data_t, meta::size_t<0>>::value)
#else
            CONCEPT_REQUIRES(Constructible<data_t, meta::size_t<0>>())
#endif
            constexpr variant()
#ifdef RANGES_WORKAROUND_MSVC_194815
              : which_(0), data_{meta::size_t<0>{}}
#else
              : variant{emplaced_index<0>}
#endif
            {}
            template<std::size_t N, typename...Args,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                CONCEPT_REQUIRES_(Constructible<data_t, meta::size_t<N>, Args...>::value)>
#else
                CONCEPT_REQUIRES_(Constructible<data_t, meta::size_t<N>, Args...>())>
#endif
            constexpr variant(RANGES_EMPLACED_INDEX_T(N), Args &&...args)
              : which_(N), data_{meta::size_t<N>{}, detail::forward<Args>(args)...}
            {
                static_assert(N < sizeof...(Ts), "");
            }
            variant(variant &&that)
              : variant{detail::empty_variant_tag{}}
            {
                this->assign_(std::move(that));
            }
            variant(variant const &that)
              : variant{detail::empty_variant_tag{}}
            {
                this->assign_(that);
            }
            variant &operator=(variant &&that)
            {
                this->clear_();
                this->assign_(std::move(that));
                return *this;
            }
            variant &operator=(variant const &that)
            {
                this->clear_();
                this->assign_(that);
                return *this;
            }
            ~variant()
            {
                this->clear_();
            }
            static constexpr std::size_t size()
            {
                return sizeof...(Ts);
            }
            template<std::size_t N, typename ...Args,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR
                CONCEPT_REQUIRES_(Constructible<data_t, meta::size_t<N>, Args...>::value)>
#else
                CONCEPT_REQUIRES_(Constructible<data_t, meta::size_t<N>, Args...>())>
#endif
            void emplace(Args &&...args)
            {
                this->clear_();
                detail::construct_fun<Args&&...> fn{std::forward<Args>(args)...};
                data_.apply(N, detail::make_unary_visitor(std::ref(fn)));
                which_ = N;
            }
            bool is_valid() const
            {
                return which() != (std::size_t)-1;
            }
            std::size_t which() const
            {
                return which_;
            }

#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
            using Args_default = meta::transform<types_t, detail::add_ref_t>;
            using Args_cdefault = meta::transform<types_t, detail::add_cref_t>;
            template<typename Fun>
            using Result_default = detail::variant_result_t<Fun, Args_default>;
            template<typename Fun>
            using Result_cdefault = detail::variant_result_t<Fun, Args_cdefault>;
            template<typename Fun>
            using Result_i_default = detail::variant_result_i_t<Fun, Args_default>;
            template<typename Fun>
            using Result_i_cdefault = detail::variant_result_i_t<Fun, Args_cdefault>;
#endif

#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
            template<typename Fun>
            Result_default<Fun> apply(Fun &&fun)
#else
            template<typename Fun,
                typename Args = meta::transform<types_t, detail::add_ref_t>,
                typename Result = detail::variant_result_t<Fun, Args>>
            Result apply(Fun fun)
#endif
            {
#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
                auto res = detail::variant_core_access::make_empty(meta::id<Result_default<Fun>>{});
#else
                auto res = detail::variant_core_access::make_empty(meta::id<Result>{});
#endif
                data_.apply(
                    which_,
                    detail::make_unary_visitor(
                        detail::unwrap_ref_fun<Fun>{detail::move(fun)},
                        res));
                RANGES_ASSERT(res.is_valid());
                return res;
            }
#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
            template<typename Fun>
            Result_cdefault<Fun> apply(Fun fun) const
#else
            template<typename Fun,
                typename Args = meta::transform<types_t, detail::add_cref_t>,
                typename Result = detail::variant_result_t<Fun, Args>>
            Result apply(Fun fun) const
#endif
            {
#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
                auto res = detail::variant_core_access::make_empty(meta::id<Result_cdefault<Fun>>{});
#else
                auto res = detail::variant_core_access::make_empty(meta::id<Result>{});
#endif
                data_.apply(
                    which_,
                    detail::make_unary_visitor(
                        detail::unwrap_ref_fun<Fun>{detail::move(fun)},
                        res));
                RANGES_ASSERT(res.is_valid());
                return res;
            }

#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
            template<typename Fun>
            Result_i_default<Fun> apply_i(Fun fun)
#else
            template<typename Fun,
                typename Args = meta::transform<types_t, detail::add_ref_t>,
                typename Result = detail::variant_result_i_t<Fun, Args>>
            Result apply_i(Fun fun)
#endif
            {
#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
                auto res = detail::variant_core_access::make_empty(meta::id<Result_i_default<Fun>>{});
#else
                auto res = detail::variant_core_access::make_empty(meta::id<Result>{});
#endif
                data_.apply(
                    which_,
                    detail::make_binary_visitor(
                        detail::unwrap_ref_fun<Fun>{detail::move(fun)},
                        res));
                RANGES_ASSERT(res.is_valid());
                return res;
            }
#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
            template<typename Fun>
            Result_i_cdefault<Fun> apply_i(Fun fun) const
#else
            template<typename Fun,
                typename Args = meta::transform<types_t, detail::add_cref_t>,
                typename Result = detail::variant_result_i_t<Fun, Args >>
            Result apply_i(Fun fun) const
#endif
            {
#ifdef RANGES_WORKAROUND_MSVC_DEFAULT_TEMPLATE_ARGUMENT
                auto res = detail::variant_core_access::make_empty(meta::id<Result_i_cdefault<Fun>>{});
#else
                auto res = detail::variant_core_access::make_empty(meta::id<Result>{});
#endif
                data_.apply(
                    which_,
                    detail::make_binary_visitor(
                        detail::unwrap_ref_fun<Fun>{detail::move(fun)},
                        res));
                RANGES_ASSERT(res.is_valid());
                return res;
            }
        };

        template<typename...Ts, typename...Us,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR_PACKEXPANSION
            CONCEPT_REQUIRES_(meta::and_c<(bool)EqualityComparable<Ts, Us>::value...>::value)>
#else
            CONCEPT_REQUIRES_(meta::and_c<(bool)EqualityComparable<Ts, Us>()...>::value)>
#endif
        bool operator==(variant<Ts...> const &lhs, variant<Us...> const &rhs)
        {
            RANGES_ASSERT(lhs.is_valid());
            RANGES_ASSERT(rhs.is_valid());
            return lhs.which() == rhs.which() &&
                detail::variant_core_access::data(lhs).equal(lhs.which(), detail::variant_core_access::data(rhs));
        }

        template<typename...Ts, typename...Us,
#ifdef RANGES_WORKAROUND_MSVC_SFINAE_CONSTEXPR_PACKEXPANSION
            CONCEPT_REQUIRES_(meta::and_c<(bool)EqualityComparable<Ts, Us>::value...>::value)>
#else
            CONCEPT_REQUIRES_(meta::and_c<(bool)EqualityComparable<Ts, Us>()...>::value)>
#endif
        bool operator!=(variant<Ts...> const &lhs, variant<Us...> const &rhs)
        {
            return !(lhs == rhs);
        }

        template<std::size_t N, typename...Ts>
        struct variant_element<N, variant<Ts...>>
          : meta::if_<
                std::is_reference<meta::at_c<meta::list<Ts...>, N>>,
                meta::id<meta::at_c<meta::list<Ts...>, N>>,
                std::decay<meta::at_c<meta::list<Ts...>, N>>>
        {};

        ////////////////////////////////////////////////////////////////////////////////////////////
        // get
        template<std::size_t N, typename...Ts>
        meta::apply<detail::add_ref_t, variant_element_t<N, variant<Ts...>>>
        get(variant<Ts...> &var)
        {
            RANGES_ASSERT(N == var.which());
            using elem_t =
                meta::_t<std::remove_reference<
                    variant_element_t<N, variant<Ts...>>>>;
            using get_fun = detail::get_fun<elem_t>;
            elem_t *elem = nullptr;
            auto &data = detail::variant_core_access::data(var);
            data.apply(N, detail::make_unary_visitor(detail::unwrap_ref_fun<get_fun>{get_fun(elem)}));
            RANGES_ASSERT(elem != nullptr);
            return *elem;
        }

        template<std::size_t N, typename...Ts>
        meta::apply<detail::add_cref_t, variant_element_t<N, variant<Ts...>>>
        get(variant<Ts...> const &var)
        {
            RANGES_ASSERT(N == var.which());
            using elem_t =
                meta::apply<
                    meta::compose<
                        meta::quote_trait<std::remove_reference>,
                        meta::quote_trait<std::add_const>
                    >, variant_element_t<N, variant<Ts...>>>;
            using get_fun = detail::get_fun<elem_t>;
            elem_t *elem = nullptr;
            auto &data = detail::variant_core_access::data(var);
            data.apply(N, detail::make_unary_visitor(detail::unwrap_ref_fun<get_fun>{get_fun(elem)}));
            RANGES_ASSERT(elem != nullptr);
            return *elem;
        }

        template<std::size_t N, typename...Ts>
        meta::_t<std::add_rvalue_reference<variant_element_t<N, variant<Ts...>>>>
        get(variant<Ts...> &&var)
        {
            RANGES_ASSERT(N == var.which());
            using elem_t =
                meta::_t<std::remove_reference<
                    variant_element_t<N, variant<Ts...>>>>;
            using get_fun = detail::get_fun<elem_t>;
            elem_t *elem = nullptr;
            auto &data = detail::variant_core_access::data(var);
            data.apply(N, detail::make_unary_visitor(detail::unwrap_ref_fun<get_fun>{get_fun(elem)}));
            RANGES_ASSERT(elem != nullptr);
            return std::forward<variant_element_t<N, variant<Ts...>>>(*elem);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // emplace
        template<std::size_t N, typename...Ts, typename...Args>
        void emplace(variant<Ts...> &var, Args &&...args)
        {
            var.template emplace<N>(std::forward<Args>(args)...);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // variant_unique
        template<typename Var>
        struct variant_unique
        {};

        template<typename ...Ts>
        struct variant_unique<variant<Ts...>>
        {
            using type =
                meta::apply_list<
                    meta::quote<variant>,
                    meta::unique<meta::list<Ts...>>>;
        };

        template<typename Var>
        using variant_unique_t = meta::_t<variant_unique<Var>>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // unique_variant
        template<typename...Ts>
        variant_unique_t<variant<Ts...>>
        unique_variant(variant<Ts...> const &var)
        {
            using From = variant<Ts...>;
            using To = variant_unique_t<From>;
            auto &data = detail::variant_core_access::data(var);
            auto res = detail::variant_core_access::make_empty(meta::id<To>{});
            data.apply(var.which(), detail::unique_visitor<To, From>{res});
            RANGES_ASSERT(res.is_valid());
            return res;
        }
        /// @}
    }
}

#endif
