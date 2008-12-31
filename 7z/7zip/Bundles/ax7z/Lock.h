#define N BOOST_PP_ITERATION()

#define VAR_TN BOOST_PP_ENUM_PARAMS(N, t)
#if   BOOST_PP_ITERATION_FLAGS() == 1
#define ARG_(z, n, data) typename boost::call_traits<typename boost::mpl::at<boost::function_types::parameter_types<Method>, boost::mpl::int_<n+1> >::type>::param_type BOOST_PP_CAT(t, n)
#elif BOOST_PP_ITERATION_FLAGS() == 2
#define ARG_(z, n, data) typename boost::call_traits<typename boost::mpl::at<boost::function_types::parameter_types<Method>, boost::mpl::long_<n+1> >::type>::param_type BOOST_PP_CAT(t, n)
#elif BOOST_PP_ITERATION_FLAGS() == 3
#define ARG_(z, n, data) typename boost::call_traits<typename boost::mpl::at_c<boost::function_types::parameter_types<Method>, n+1>::type>::param_type BOOST_PP_CAT(t, n)
#endif
#define ARG_N BOOST_PP_ENUM_TRAILING(N, ARG_, _)

	template<typename Method>
	typename boost::enable_if_c<boost::function_types::function_arity<Method>::value == N + 1, typename boost::function_types::result_type<Method>::type>::type
	CallWithLockGuard(Method m /*,*/ ARG_N)
	{
		boost::lock_guard<SolidCache::Mutex> guard(SolidCache::GetMutex());
		return (this->*m)(VAR_TN);
	}
	template<typename Method>
	typename boost::enable_if_c<boost::function_types::function_arity<Method>::value == N + 1, typename boost::function_types::result_type<Method>::type>::type
	CallWithLockGuard(Method m /*,*/ ARG_N) const
	{
		boost::lock_guard<SolidCache::Mutex> guard(SolidCache::GetMutex());
		return (this->*m)(VAR_TN);
	}
	template<typename Method>
	typename boost::enable_if_c<boost::function_types::function_arity<Method>::value == N + 1, typename boost::function_types::result_type<Method>::type>::type
	CallWithSharedLock(Method m /*,*/ ARG_N)
	{
		boost::shared_lock<SolidCache::Mutex> guard(SolidCache::GetMutex());
		return (this->*m)(VAR_TN);
	}
	template<typename Method>
	typename boost::enable_if_c<boost::function_types::function_arity<Method>::value == N + 1, typename boost::function_types::result_type<Method>::type>::type
	CallWithSharedLock(Method m /*,*/ ARG_N) const
	{
		boost::shared_lock<SolidCache::Mutex> guard(SolidCache::GetMutex());
		return (this->*m)(VAR_TN);
	}

#undef ARG_N
#undef ARG_
#undef VAR_TN
#undef N
