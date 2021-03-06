#pragma once

#include <type_traits>

#include <vap\iterators\iterators.h>
#include <vap\execution_policy.h>

namespace vap		  {
namespace expressions {

// Forward declare all expression-derived classes.
template <class Derived>
class Expression;

template <typename Left, typename Right, class Operator, class Exec>
class Binary;

template <typename Exp, class Operator, class Exec>
class Unary;

template <typename Type>
class Scalar;

template <typename T, typename ConstructPolicy, typename Container, typename Exec>
class vector;

} // end namespace expressions

namespace detail {

// General type is not an expression
template <typename T>
struct is_expression : std::false_type{};

// Specialize expression and expression-derived types to hold true
template <class D>
struct is_expression <expressions::Expression<D>>			: std::true_type{};

template <class L, class R, class O, class P>
struct is_expression <expressions::Binary<L, R, O, P>>		: std::true_type{};

template <typename E, class O, class P>
struct is_expression <expressions::Unary<E, O, P>>			: std::true_type{};

template <typename T>
struct is_expression <expressions::Scalar<T>>				: std::true_type{};

template <class T, class Ctor, class C, class P>
struct is_expression <expressions::vector<T, Ctor, C, P>>	: std::true_type{};

// For ensuring the type will have class-like properties
template <typename T>
using vectorize = std::conditional<std::is_arithmetic<T>::value, 
								   expressions::Scalar<T>,		 // If T is an arithmetic type, wrap it in a Scalar<T>
								   T>;					         // Otherwise, T is a possible expression candidate
    
template <typename T>
using vectorize_t = typename vectorize<T>::type;
    
template <typename T>
struct is_exec : std::false_type{};
    
template <>
struct is_exec<execution_policy>   : std::true_type{};
    
template <>
struct is_exec<absorption_policy>  : std::true_type{};

template <>
struct is_exec<serial_execution>   : std::true_type{};
    
template <>
struct is_exec<parallel_execution> : std::true_type{};
    
template <typename Exec>
struct is_strong_exec 
{
    static const bool value = std::is_same<Exec, parallel_execution>::value ||
			                  std::is_same<Exec, serial_execution>::value;
};

template <typename Exec>
struct is_weak_exec
{
	static const bool value = std::is_same<Exec, execution_policy>::value ||
							  std::is_same<Exec, absorption_policy>::value;
};


template <typename T>
struct exec_extractor
{
	using type = typename T::exec;
};

// Choose the strongest execution policy from the template parameters
template <typename T1, typename... Ts>
struct get_strongest_exec
{
	using type = std::conditional_t<
					is_strong_exec<T1>::value,	                // Determine if T1 is strong or not
					T1,											// If T1 is strong, return it
					typename get_strongest_exec<Ts...>::type>;	// Otherwise, recurse down the list until we've found the strongest exec
};

// Edge case of get_strongest_exec
template <typename T1>
struct get_strongest_exec<T1>
{
	using type = std::conditional_t<
					is_strong_exec<T1>::value,	// Check if T1 is a strong execution policy
					T1,							// If T1 is strong, return T1
					absorption_policy>;			// Otherwise, absorb surrounding execution policies
};

template <typename... Ts>
struct get_exec;

template <typename T>
struct get_exec<T> : get_strongest_exec<typename T::exec>{};

// *** MSVC COMPILER BUG: https://connect.microsoft.com/VisualStudio/feedback/details/810018/variadic-template-parameter-pack-expansion-error
// ***					  https://connect.microsoft.com/VisualStudio/feedback/details/862959/c-parameter-pack-expansion-fails
// *** We can't expand parameter pack with a subtype specified. Although the issue has been fixed in a later MSVC, we must work around this bug.
// *** The current work-around uses exec_extractor

//template <typename T, typename ... Ts>
//struct get_exec<T, Ts...> : get_strongest_exec<typename T::exec,
											   //typename Ts::exec...> {};

template <typename T, typename ... Ts>
struct get_exec<T, Ts...> : get_strongest_exec<typename exec_extractor<T>::type,
											   typename exec_extractor<Ts>::type...>{};

// If either of the execution policies inherit, then return true.
// Otherwise compare execution policies to be equal.
template <typename Exec1, typename Exec2>
struct compatible_execs
{
	static const bool value = std::is_same<Exec1, absorption_policy>::value ||
							  std::is_same<Exec2, absorption_policy>::value ||
							  std::is_same<Exec1, Exec2>::value;
};

} // end namespace detail

namespace expressions {

template <typename T>
struct expression_traits;

// Defines expression_traits for derived specialization Binary
template <typename L, typename R, class Op, class Exec>
struct expression_traits<vap::expressions::Binary<L, R, Op, Exec>>
{
	using exec				   = typename detail::get_strongest_exec<Exec, typename L::exec, typename R::exec>::type;
	using value_type		   = typename L::value_type;

	using left_iterator		   = typename L::iterator;
	using left_const_iterator  = typename L::const_iterator;

	using right_iterator	   = typename R::iterator;
	using right_const_iterator = typename R::const_iterator;

	// Retrieve binary_iterator specialization type for the given execution policy
	using iterator	= typename iterators
		  ::binary_iterator<exec>
		  ::template type<Op, left_iterator, right_iterator>; 
	
	// Retrieve const binary_iterator specialization type for the given execution policy
	using const_iterator = typename iterators
		  ::binary_iterator<exec>
		  ::template type<Op, left_const_iterator, right_const_iterator>; 
};

// Defines expression_traits for derived specialization Unary
template <typename T, class Op, class Exec>
struct expression_traits<vap::expressions::Unary<T, Op, Exec>>
{
	using exec				  = typename detail::get_strongest_exec<Exec, typename T::exec>::type;
	using value_type		  = typename T::value_type;
	using base_iterator		  = typename T::iterator;
	using const_base_iterator = typename T::const_iterator;
	
	// Retrieve unary_iterator specialization type for the given execution policy
	using iterator = typename iterators
		  ::unary_iterator<exec>
		  ::template type<Op, base_iterator>;
	
	// Retrieve const unary_iterator specialization type for the given execution policy
	using const_iterator = typename iterators
		  ::unary_iterator<exec>
		  ::template type<Op, const_base_iterator>;
};

 //Defines expression_traits for derived specialization Unary
template <typename T>
struct expression_traits<vap::expressions::Scalar<T>>
{
	using exec				  = vap::absorption_policy;
	using value_type		  = T;
	
	// Retrieve unary_iterator specialization type for the given execution policy
	using iterator = typename iterators
		  ::scalar_iterator<exec>
		  ::template type<T>;
	
	// Retrieve const unary_iterator specialization type for the given execution policy
	using const_iterator = typename iterators
		  ::scalar_iterator<exec>
		  ::template type<T>;
};

template <typename T, class Ctor, typename C, class Exec>
struct expression_traits<vap::expressions::vector<T, Ctor, C, Exec>>
{
	using value_type	 = T;
	using exec			 = Exec;
	using iterator		 = typename C::iterator;
	using const_iterator = typename C::const_iterator;
};

} // end namespace expressions

using detail::is_expression;

using detail::vectorize;
using detail::vectorize_t;
    
using detail::get_exec;
using detail::get_strongest_exec;

using detail::compatible_execs;

} // end namespace vap
