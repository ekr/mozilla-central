/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2013, Mozilla
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <nr_api.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <assert.h>

#include "p_buf.h"
#include "nr_socket.h"
#include "nr_socket_buffered.h"


typedef struct nr_socket_buffered_ {
  nr_socket *inner;
  nr_p_buf_ctx *p_bufs;
  nr_p_buf_head pending_writes;
  size_t pending;
  size_t max_pending;
} nr_socket_buffered;

static int nr_socket_buffered_destroy(void **objp);
static int nr_socket_buffered_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *to);
static int nr_socket_buffered_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from);
static int nr_socket_buffered_getfd(void *obj, NR_SOCKET *fd);
static int nr_socket_buffered_getaddr(void *obj, nr_transport_addr *addrp);
static int nr_socket_buffered_close(void *obj);
static int nr_socket_buffered_connect(void *sock, nr_transport_addr *addr);
static int nr_socket_buffered_write(void *obj,const void *msg, size_t len, size_t *written);
static int nr_socket_buffered_read(void *obj,void * restrict buf, size_t maxlen, size_t *len);
static void nr_socket_buffered_writable_cb(NR_SOCKET s, int how, void *arg);

static nr_socket_vtbl nr_socket_buffered_vtbl={
  nr_socket_buffered_destroy,
  nr_socket_buffered_sendto,
  nr_socket_buffered_recvfrom,
  nr_socket_buffered_getfd,
  nr_socket_buffered_getaddr,
  nr_socket_buffered_close,
  nr_socket_buffered_connect,
  nr_socket_buffered_write,
  nr_socket_buffered_read
};


int nr_socket_buffered_create(nr_socket *inner, int max_pending, nr_socket **sockp)
{
  int r, _status;
  nr_socket_buffered *sock = 0;

  if (!(sock = RCALLOC(sizeof(nr_socket_buffered))))
    ABORT(R_NO_MEMORY);

  STAILQ_INIT(&sock->pending_writes);
  if ((r=nr_p_buf_ctx_create(4096, &sock->p_bufs)))
    ABORT(r);
  sock->max_pending=max_pending;
  sock->inner = inner;

  if ((r=nr_socket_create_int(sock, &nr_socket_buffered_vtbl, sockp)))
    ABORT(r);

  _status=0;
abort:
  if (_status) {
    void *sock_v = sock;
    nr_socket_buffered_destroy(&sock_v);
  }
  return(_status);
}

/* Note: This does not destroy the inner socket */
int nr_socket_buffered_destroy(void **objp)
{
  nr_socket_buffered *sock;

  if (!objp || !*objp)
    return 0;

  sock = (nr_socket_buffered *)*objp;
  *objp = 0;

  /* TODO(ekr@rtfm.com): Cancel waiting on socket */
  nr_p_buf_free_chain(sock->p_bufs, &sock->pending_writes);
  nr_p_buf_ctx_destroy(&sock->p_bufs);

  RFREE(sock);

  return 0;
}

static int nr_socket_buffered_sendto(void *obj,const void *msg, size_t len,
  int flags, nr_transport_addr *to)
{
  int r, _status;
  size_t written;

  if ((r=nr_socket_buffered_write(obj, msg, len, &written)))
    ABORT(r);

  if (len != written)
    ABORT(R_IO_ERROR);

  _status=0;
abort:
  return _status;
}

static int nr_socket_buffered_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from)
{
  assert(0);
  return R_INTERNAL;
}

static int nr_socket_buffered_getfd(void *obj, NR_SOCKET *fd)
{
  nr_socket_buffered *sock = (nr_socket_buffered *)obj;

  return nr_socket_getfd(sock->inner, fd);
}

static int nr_socket_buffered_getaddr(void *obj, nr_transport_addr *addrp)
{
  nr_socket_buffered *sock = (nr_socket_buffered *)obj;

  return nr_socket_getaddr(sock->inner, addrp);
}

static int nr_socket_buffered_close(void *obj)
{
  /* No-op */
  return 0;
}

static int nr_socket_buffered_connect(void *sock, nr_transport_addr *addr)
{
  assert(0);
  return R_INTERNAL;
}

static int nr_socket_buffered_write(void *obj,const void *msg, size_t len, size_t *written)
{
  nr_socket_buffered *sock = (nr_socket_buffered *)obj;
  int already_armed = 0;
  int r,_status;
  size_t written2 = 0;

  if (!sock->pending) {
    r = nr_socket_write(sock->inner, msg, len, &written2, 0);
    if (r) {
      if (r != R_WOULDBLOCK)
        ABORT(r);

      written2=0;
    }
  } else {
    already_armed = 1;
  }

  /* Buffer what's left */
  len -= written2;

  if (len) {
    /* Buffers are full, report error */
    if ((sock->pending + len) > sock->max_pending) {
      r_log(LOG_GENERIC, LOG_INFO, "Buffers full");
      ABORT(R_IO_ERROR);
    }

    if (len) {
      if ((r=nr_p_buf_write_to_chain(sock->p_bufs, &sock->pending_writes,
                                     ((UCHAR *)msg) + written2, len)))
        ABORT(r);
    }

    sock->pending += len;
  }

  if (sock->pending && !already_armed) {
    NR_SOCKET fd;

    if ((r=nr_socket_getfd(sock->inner, &fd)))
      ABORT(r);

    NR_ASYNC_WAIT(fd, NR_ASYNC_WAIT_WRITE, nr_socket_buffered_writable_cb, sock);
  }

  *written = written2;

  _status=0;
abort:
  return _status;
}

static void nr_socket_buffered_writable_cb(NR_SOCKET s, int how, void *arg)
{
  nr_socket_buffered *sock = (nr_socket_buffered *)arg;
  int r,_status;
  nr_p_buf *n1, *n2;

  /* Try to flush */
  STAILQ_FOREACH_SAFE(n1, &sock->pending_writes, entry, n2) {
    size_t written = 0;

    if ((r=nr_socket_write(sock->inner, n1->data + n1->r_offset,
                           n1->length - n1->r_offset,
                           &written, 0))) {

      ABORT(r);
    }

    n1->r_offset += written;
    if (n1->r_offset <= n1->length) {
      /* We wrote something, but not everything */
      ABORT(R_WOULDBLOCK);
    }

    /* We are done with this p_buf */
    STAILQ_REMOVE_HEAD(&sock->pending_writes, entry);
    nr_p_buf_free(sock->p_bufs, n1);
  }

  _status=0;
abort:
  if (_status && _status != R_WOULDBLOCK) {
    /* TODO(ekr@rtfm.com): Mark the socket as failed */
  }
}

static int nr_socket_buffered_read(void *obj,void * restrict buf, size_t maxlen, size_t *len)
{
  nr_socket_buffered *sock = (nr_socket_buffered *)obj;

  return nr_socket_read(sock->inner, buf, maxlen, len, 0);
}

