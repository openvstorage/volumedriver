// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

/*
 * use_rs.h
 *
 *  Created on: Jun 30, 2015
 *      Author: arne
 */

#ifndef USE_RS_H_
#define USE_RS_H_

#define rs_socket(f,t,p)  use_rs_ ? ::rsocket(f,t,p)  : ::socket(f,t,p)
#define rs_bind(s,a,l)    use_rs_ ? ::rbind(s,a,l)    : ::bind(s,a,l)
#define rs_listen(s,b)    use_rs_ ? ::rlisten(s,b)    : ::listen(s,b)
#define rs_connect(s,a,l) use_rs_ ? ::rconnect(s,a,l) : ::connect(s,a,l)
#define rs_accept(s,a,l)  use_rs_ ? ::raccept(s,a,l)  : ::accept(s,a,l)
#define rs_shutdown(s,h)  use_rs_ ? ::rshutdown(s,h)  : ::shutdown(s,h)
#define rs_close(s)       use_rs_ ? ::rclose(s)       : ::close(s)
#define rs_recv(s,b,l,f)  use_rs_ ? ::rrecv(s,b,l,f)  : ::recv(s,b,l,f)
#define rs_send(s,b,l,f)  use_rs_ ? ::rsend(s,b,l,f)  : ::send(s,b,l,f)
#define rs_poll(f,n,t)	  use_rs_ ? ::rpoll(f,n,t)	  : ::poll(f,n,t)
#define rs_fcntl2(s,c)    use_rs_ ? ::rfcntl(s,c)     : ::fcntl(s,c)
#define rs_fcntl3(s,c,p)  use_rs_ ? ::rfcntl(s,c,p)   : ::fcntl(s,c,p)
#define rs_setsockopt(s,l,n,v,ol) use_rs_ ? ::rsetsockopt(s,l,n,v,ol) : ::setsockopt(s,l,n,v,ol)
#define rs_getsockopt(s,l,n,v,ol) use_rs_ ? ::rgetsockopt(s,l,n,v,ol) : ::getsockopt(s,l,n,v,ol)
#define rs_getpeername(s,a,l)     use_rs_ ? ::rgetpeername(s,a,l)     : ::getpeername(s,a,l)

#endif /* USE_RS_H_ */
