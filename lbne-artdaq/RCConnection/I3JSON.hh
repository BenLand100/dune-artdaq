
// JCF, Apr-2-2015

// The following is my repurposing of IceCube code sent to me by Erik
// Blaufuss; it's designed to make it easy to turn std::map-s (and other
// types of C++ object) into JSON code


/**
 * copyright  (C) 2004
 * the icecube collaboration
 * $Id: I3JSON.h 12673 2005-11-17 10:36:58Z tschmidt $
 *
 * @file I3JSON.h
 * @version $Revision: 1.10 $
 * @date $Date: 2005-12-01 16:45:55 -0500 (Thu, 01 Dec 2005) $
 * @author tschmidt 
 */
#ifndef I3JSON_H_INCLUDED
#define I3JSON_H_INCLUDED


#include <map>
#include <ostream>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include <boost/none.hpp>
#include <boost/version.hpp>
#if (BOOST_VERSION < 104700) && ((GCC_VERSION >= 40500) || (defined(__clang__)))
#define BOOST_VARIANT_NO_FULL_RECURSIVE_VARIANT_SUPPORT
#endif
#include <boost/variant.hpp>


namespace lbne
{
  enum null_t
  {
    Null = 0
  };

  enum boolean_t
  {
    False = 0,
    True = 1
  };

  /** @brief This type(-def) stores JSON data that is converted into a string
   *  using an output stream operator.
   * 
   * @note
   * It is a very simple <em>implementation</em> that is primarily used
   * to store small objects such as alert messages (cp. I3AlertService).
   * 
   * @note
   * value_t is a recursive type definition. It may store nothing (null_t),
   * booleans (boolean_t), numbers, strings, a series value_t values (array_t)
   * or a map with string-type keys and value_t values (object_t). 
   *    
   * @note
   * It would be nice to use bool instead of boolean_t for boolean data,
   * but one couldn't use code such as <em>someValue = "wow";</em> to assign
   * a string in that case.
   * The previous assignment would be equivalent to assigning a boolean
   * - only <em>someValue = std::string("wow");</em> assigns a string to
   * <em>someValue</em>.
   * So, <em>someValue = "wow";</em> assigns a string to a JSON value as it
   * is currently defined!
   */
  typedef boost::make_recursive_variant<null_t,
                                        boolean_t,
                                        int,
                                        unsigned int,
                                        long long int,
                                        unsigned long long int,
                                        double,
                                        std::string,
                                        std::vector<boost::recursive_variant_>, // eq. to array_t
                                        std::map<std::string,                   // eq. to object_t
                                                 boost::recursive_variant_>
                                       >::type value_t;
  typedef std::vector<value_t> array_t;
  typedef std::map<std::string, value_t> object_t;

  template<typename T>
  value_t ToJSON(const T& val)
  {
    return(val.ToJSON());
  }
  template<typename T>
  value_t ToJSON(const boost::optional<T>& val)
  {
    if(!val) return(Null);
    else return(ToJSON(val.get()));
  }

std::ostream& operator<<(std::ostream& os, const value_t& value);

  // JCF, 4/29/15

  // I've added this function, MsgToRCJSON, so its users can specify a
  // message (with a label) but not have to worry about details of
  // packaging it up into a JSON string for RunControl's consumption

  std::string MsgToRCJSON(const std::string& label, const std::string& msg, 
			  const std::string& severity);

}


#endif /*I3JSON_H_INCLUDED*/
