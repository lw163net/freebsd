/*-
 * Copyright (c) 1994-1995 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_util.h>

#ifndef __alpha__
int
linux_creat(struct proc *p, struct linux_creat_args *args)
{
    struct open_args /* {
	char *path;
	int flags;
	int mode;
    } */ bsd_open_args;
    caddr_t sg;

    sg = stackgap_init();
    CHECKALTCREAT(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(creat))
		printf(ARGS(creat, "%s, %d"), args->path, args->mode);
#endif
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;
    bsd_open_args.flags = O_WRONLY | O_CREAT | O_TRUNC;
    return open(p, &bsd_open_args);
}
#endif /*!__alpha__*/

int
linux_open(struct proc *p, struct linux_open_args *args)
{
    struct open_args /* {
	char *path;
	int flags;
	int mode;
    } */ bsd_open_args;
    int error;
    caddr_t sg;

    sg = stackgap_init();
    
    if (args->flags & LINUX_O_CREAT)
	CHECKALTCREAT(p, &sg, args->path);
    else
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(open))
		printf(ARGS(open, "%s, 0x%x, 0x%x"),
		    args->path, args->flags, args->mode);
#endif
    bsd_open_args.flags = 0;
    if (args->flags & LINUX_O_RDONLY)
	bsd_open_args.flags |= O_RDONLY;
    if (args->flags & LINUX_O_WRONLY) 
	bsd_open_args.flags |= O_WRONLY;
    if (args->flags & LINUX_O_RDWR)
	bsd_open_args.flags |= O_RDWR;
    if (args->flags & LINUX_O_NDELAY)
	bsd_open_args.flags |= O_NONBLOCK;
    if (args->flags & LINUX_O_APPEND)
	bsd_open_args.flags |= O_APPEND;
    if (args->flags & LINUX_O_SYNC)
	bsd_open_args.flags |= O_FSYNC;
    if (args->flags & LINUX_O_NONBLOCK)
	bsd_open_args.flags |= O_NONBLOCK;
    if (args->flags & LINUX_FASYNC)
	bsd_open_args.flags |= O_ASYNC;
    if (args->flags & LINUX_O_CREAT)
	bsd_open_args.flags |= O_CREAT;
    if (args->flags & LINUX_O_TRUNC)
	bsd_open_args.flags |= O_TRUNC;
    if (args->flags & LINUX_O_EXCL)
	bsd_open_args.flags |= O_EXCL;
    if (args->flags & LINUX_O_NOCTTY)
	bsd_open_args.flags |= O_NOCTTY;
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;

    error = open(p, &bsd_open_args);
    PROC_LOCK(p);
    if (!error && !(bsd_open_args.flags & O_NOCTTY) && 
	SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
	struct filedesc *fdp = p->p_fd;
	struct file *fp = fdp->fd_ofiles[p->p_retval[0]];

	PROC_UNLOCK(p);
	if (fp->f_type == DTYPE_VNODE)
	    fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0, p);
    } else
	PROC_UNLOCK(p);
#ifdef DEBUG
	if (ldebug(open))
		printf(LMSG("open returns error %d"), error);
#endif
    return error;
}

struct linux_flock {
    short l_type;
    short l_whence;
    linux_off_t l_start;
    linux_off_t l_len;
    linux_pid_t l_pid;
};

static void
linux_to_bsd_flock(struct linux_flock *linux_flock, struct flock *bsd_flock)
{
    switch (linux_flock->l_type) {
    case LINUX_F_RDLCK:
	bsd_flock->l_type = F_RDLCK;
	break;
    case LINUX_F_WRLCK:
	bsd_flock->l_type = F_WRLCK;
	break;
    case LINUX_F_UNLCK:
	bsd_flock->l_type = F_UNLCK;
	break;
    default:
        bsd_flock->l_type = -1;
        break;
    }
    bsd_flock->l_whence = linux_flock->l_whence;
    bsd_flock->l_start = (off_t)linux_flock->l_start;
    bsd_flock->l_len = (off_t)linux_flock->l_len;
    bsd_flock->l_pid = (pid_t)linux_flock->l_pid;
}

static void
bsd_to_linux_flock(struct flock *bsd_flock, struct linux_flock *linux_flock)
{
    switch (bsd_flock->l_type) {
    case F_RDLCK:
	linux_flock->l_type = LINUX_F_RDLCK;
	break;
    case F_WRLCK:
	linux_flock->l_type = LINUX_F_WRLCK;
	break;
    case F_UNLCK:
	linux_flock->l_type = LINUX_F_UNLCK;
	break;
    }
    linux_flock->l_whence = bsd_flock->l_whence;
    linux_flock->l_start = (linux_off_t)bsd_flock->l_start;
    linux_flock->l_len = (linux_off_t)bsd_flock->l_len;
    linux_flock->l_pid = (linux_pid_t)bsd_flock->l_pid;
}

int
linux_fcntl(struct proc *p, struct linux_fcntl_args *args)
{
    int error, result;
    struct fcntl_args /* {
	int fd;
	int cmd;
	long arg;
    } */ fcntl_args;
    struct linux_flock linux_flock;
    struct flock *bsd_flock;
    struct filedesc *fdp;
    struct file *fp;
    caddr_t sg;

    sg = stackgap_init();
    bsd_flock = (struct flock *)stackgap_alloc(&sg, sizeof(struct flock));

#ifdef DEBUG
	if (ldebug(fcntl))
		printf(ARGS(fcntl, "%d, %08x, *"), args->fd, args->cmd);
#endif
    fcntl_args.fd = args->fd;

    switch (args->cmd) {
    case LINUX_F_DUPFD:
	fcntl_args.cmd = F_DUPFD;
	fcntl_args.arg = args->arg;
	return fcntl(p, &fcntl_args);

    case LINUX_F_GETFD:
	fcntl_args.cmd = F_GETFD;
	return fcntl(p, &fcntl_args);

    case LINUX_F_SETFD:
	fcntl_args.cmd = F_SETFD;
	fcntl_args.arg = args->arg;
	return fcntl(p, &fcntl_args);

    case LINUX_F_GETFL:
	fcntl_args.cmd = F_GETFL;
	error = fcntl(p, &fcntl_args);
	result = p->p_retval[0];
	p->p_retval[0] = 0;
	if (result & O_RDONLY) p->p_retval[0] |= LINUX_O_RDONLY;
	if (result & O_WRONLY) p->p_retval[0] |= LINUX_O_WRONLY;
	if (result & O_RDWR) p->p_retval[0] |= LINUX_O_RDWR;
	if (result & O_NDELAY) p->p_retval[0] |= LINUX_O_NONBLOCK;
	if (result & O_APPEND) p->p_retval[0] |= LINUX_O_APPEND;
	if (result & O_FSYNC) p->p_retval[0] |= LINUX_O_SYNC;
	if (result & O_ASYNC) p->p_retval[0] |= LINUX_FASYNC;
	return error;

    case LINUX_F_SETFL:
	fcntl_args.arg = 0;
	if (args->arg & LINUX_O_NDELAY) fcntl_args.arg |= O_NONBLOCK;
	if (args->arg & LINUX_O_APPEND) fcntl_args.arg |= O_APPEND;
	if (args->arg & LINUX_O_SYNC) fcntl_args.arg |= O_FSYNC;
	if (args->arg & LINUX_FASYNC) fcntl_args.arg |= O_ASYNC;
	fcntl_args.cmd = F_SETFL;
	return fcntl(p, &fcntl_args);

    case LINUX_F_GETLK:
	if ((error = copyin((caddr_t)args->arg, (caddr_t)&linux_flock,
		   	    sizeof(struct linux_flock)))) 
	    return error;
	linux_to_bsd_flock(&linux_flock, bsd_flock);
	fcntl_args.cmd = F_GETLK;
	fcntl_args.arg = (long)bsd_flock;
	error = fcntl(p, &fcntl_args);
	if (error)
	    return error;
	bsd_to_linux_flock(bsd_flock, &linux_flock);
	return copyout((caddr_t)&linux_flock, (caddr_t)args->arg,
		       sizeof(struct linux_flock));

    case LINUX_F_SETLK:
	if ((error = copyin((caddr_t)args->arg, (caddr_t)&linux_flock,
		   	    sizeof(struct linux_flock)))) 
	    return error;
	linux_to_bsd_flock(&linux_flock, bsd_flock);
	fcntl_args.cmd = F_SETLK;
	fcntl_args.arg = (long)bsd_flock;
	return fcntl(p, &fcntl_args);

    case LINUX_F_SETLKW:
	if ((error = copyin((caddr_t)args->arg, (caddr_t)&linux_flock,
		   	    sizeof(struct linux_flock)))) 
	    return error;
	linux_to_bsd_flock(&linux_flock, bsd_flock);
	fcntl_args.cmd = F_SETLKW;
	fcntl_args.arg = (long)bsd_flock;
	return fcntl(p, &fcntl_args);

    case LINUX_F_GETOWN:
	fcntl_args.cmd = F_GETOWN;
	return fcntl(p, &fcntl_args);

    case LINUX_F_SETOWN:
	/*
	 * XXX some Linux applications depend on F_SETOWN having no
	 * significant effect for pipes (SIGIO is not delivered for
	 * pipes under Linux-2.2.35 at least).
	 */
	fdp = p->p_fd;
	if ((u_int)args->fd >= fdp->fd_nfiles ||
	  (fp = fdp->fd_ofiles[args->fd]) == NULL)
	    return EBADF;
	if (fp->f_type == DTYPE_PIPE)
	    return EINVAL;

 	fcntl_args.cmd = F_SETOWN;
	fcntl_args.arg = args->arg;
	return fcntl(p, &fcntl_args);
    }
    return EINVAL;
}

int
linux_lseek(struct proc *p, struct linux_lseek_args *args)
{

    struct lseek_args /* {
	int fd;
	int pad;
	off_t offset;
	int whence;
    } */ tmp_args;
    int error;

#ifdef DEBUG
	if (ldebug(lseek))
		printf(ARGS(lseek, "%d, %ld, %d"),
		    args->fdes, args->off, args->whence);
#endif
    tmp_args.fd = args->fdes;
    tmp_args.offset = (off_t)args->off;
    tmp_args.whence = args->whence;
    error = lseek(p, &tmp_args);
    return error;
}

#ifndef __alpha__
int
linux_llseek(struct proc *p, struct linux_llseek_args *args)
{
	struct lseek_args bsd_args;
	int error;
	off_t off;

#ifdef DEBUG
	if (ldebug(llseek))
		printf(ARGS(llseek, "%d, %d:%d, %d"),
		    args->fd, args->ohigh, args->olow, args->whence);
#endif
	off = (args->olow) | (((off_t) args->ohigh) << 32);

	bsd_args.fd = args->fd;
	bsd_args.offset = off;
	bsd_args.whence = args->whence;

	if ((error = lseek(p, &bsd_args)))
		return error;

	if ((error = copyout(p->p_retval, (caddr_t)args->res, sizeof (off_t))))
		return error;

	p->p_retval[0] = 0;
	return 0;
}
#endif /*!__alpha__*/


struct linux_dirent {
    long dino;
    linux_off_t doff;
    unsigned short dreclen;
    char dname[LINUX_NAME_MAX + 1];
};

#define LINUX_RECLEN(de,namlen) \
    ALIGN((((char *)&(de)->dname - (char *)de) + (namlen) + 1))

#ifndef __alpha__
int
linux_readdir(struct proc *p, struct linux_readdir_args *args)
{
	struct linux_getdents_args lda;

	lda.fd = args->fd;
	lda.dent = args->dent;
	lda.count = 1;
	return linux_getdents(p, &lda);
}
#endif /*!__alpha__*/

int
linux_getdents(struct proc *p, struct linux_getdents_args *args)
{
    register struct dirent *bdp;
    struct vnode *vp;
    caddr_t inp, buf;		/* BSD-format */
    int len, reclen;		/* BSD-format */
    caddr_t outp;		/* Linux-format */
    int resid, linuxreclen=0;	/* Linux-format */
    struct file *fp;
    struct uio auio;
    struct iovec aiov;
    struct vattr va;
    off_t off;
    struct linux_dirent linux_dirent;
    int buflen, error, eofflag, nbytes, justone;
    u_long *cookies = NULL, *cookiep;
    int ncookies;

#ifdef DEBUG
	if (ldebug(getdents))
		printf(ARGS(getdents, "%d, *, %d"), args->fd, args->count);
#endif
    if ((error = getvnode(p->p_fd, args->fd, &fp)) != 0) {
	return (error);
    }

    if ((fp->f_flag & FREAD) == 0)
	return (EBADF);

    vp = (struct vnode *) fp->f_data;

    if (vp->v_type != VDIR)
	return (EINVAL);

    if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p))) {
	return error;
    }

    nbytes = args->count;
    if (nbytes == 1) {
	nbytes = sizeof (struct linux_dirent);
	justone = 1;
    }
    else
	justone = 0;

    off = fp->f_offset;
#define	DIRBLKSIZ	512		/* XXX we used to use ufs's DIRBLKSIZ */
    buflen = max(DIRBLKSIZ, nbytes);
    buflen = min(buflen, MAXBSIZE);
    buf = malloc(buflen, M_TEMP, M_WAITOK);
    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
again:
    aiov.iov_base = buf;
    aiov.iov_len = buflen;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_rw = UIO_READ;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_procp = p;
    auio.uio_resid = buflen;
    auio.uio_offset = off;

    if (cookies) {
	free(cookies, M_TEMP);
	cookies = NULL;
    }

    error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &ncookies, &cookies);
    if (error) {
	goto out;
    }

    inp = buf;
    outp = (caddr_t) args->dent;
    resid = nbytes;
    if ((len = buflen - auio.uio_resid) <= 0) {
	goto eof;
    }

    cookiep = cookies;

    if (cookies) {
	/*
	 * When using cookies, the vfs has the option of reading from
	 * a different offset than that supplied (UFS truncates the
	 * offset to a block boundary to make sure that it never reads
	 * partway through a directory entry, even if the directory
	 * has been compacted).
	 */
	while (len > 0 && ncookies > 0 && *cookiep <= off) {
	    bdp = (struct dirent *) inp;
	    len -= bdp->d_reclen;
	    inp += bdp->d_reclen;
	    cookiep++;
	    ncookies--;
	}
    }

    while (len > 0) {
	if (cookiep && ncookies == 0)
	    break;
	bdp = (struct dirent *) inp;
	reclen = bdp->d_reclen;
	if (reclen & 3) {
	    printf("linux_readdir: reclen=%d\n", reclen);
	    error = EFAULT;
	    goto out;
	}
  
	if (bdp->d_fileno == 0) {
	    inp += reclen;
	    if (cookiep) {
		off = *cookiep++;
		ncookies--;
	    } else
		off += reclen;
	    len -= reclen;
	    continue;
	}
	linuxreclen = LINUX_RECLEN(&linux_dirent, bdp->d_namlen);
	if (reclen > len || resid < linuxreclen) {
	    outp++;
	    break;
	}
	linux_dirent.dino = (long) bdp->d_fileno;
	if (justone) {
	    /*
	     * old linux-style readdir usage.
	     */
	    linux_dirent.doff = (linux_off_t) linuxreclen;
	    linux_dirent.dreclen = (u_short) bdp->d_namlen;
	} else {
	    if (cookiep)
		linux_dirent.doff = (linux_off_t)*cookiep;
	    else
		linux_dirent.doff = (linux_off_t)(off + reclen);
	    linux_dirent.dreclen = (u_short) linuxreclen;
	}
	strcpy(linux_dirent.dname, bdp->d_name);
	if ((error = copyout((caddr_t)&linux_dirent, outp, linuxreclen))) {
	    goto out;
	}
	inp += reclen;
	if (cookiep) {
	    off = *cookiep++;
	    ncookies--;
	} else
	    off += reclen;
	outp += linuxreclen;
	resid -= linuxreclen;
	len -= reclen;
	if (justone)
	    break;
    }

    if (outp == (caddr_t) args->dent)
	goto again;
    fp->f_offset = off;

    if (justone)
	nbytes = resid + linuxreclen;

eof:
    p->p_retval[0] = nbytes - resid;
out:
    if (cookies)
	free(cookies, M_TEMP);
    VOP_UNLOCK(vp, 0, p);
    free(buf, M_TEMP);
    return error;
}

/*
 * These exist mainly for hooks for doing /compat/linux translation.
 */

int
linux_access(struct proc *p, struct linux_access_args *args)
{
	struct access_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(access))
		printf(ARGS(access, "%s, %d"), args->path, args->flags);
#endif
	bsd.path = args->path;
	bsd.flags = args->flags;

	return access(p, &bsd);
}

int
linux_unlink(struct proc *p, struct linux_unlink_args *args)
{
	struct unlink_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(unlink))
		printf(ARGS(unlink, "%s"), args->path);
#endif
	bsd.path = args->path;

	return unlink(p, &bsd);
}

int
linux_chdir(struct proc *p, struct linux_chdir_args *args)
{
	struct chdir_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(chdir))
		printf(ARGS(chdir, "%s"), args->path);
#endif
	bsd.path = args->path;

	return chdir(p, &bsd);
}

int
linux_chmod(struct proc *p, struct linux_chmod_args *args)
{
	struct chmod_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(chmod))
		printf(ARGS(chmod, "%s, %d"), args->path, args->mode);
#endif
	bsd.path = args->path;
	bsd.mode = args->mode;

	return chmod(p, &bsd);
}

int
linux_chown(struct proc *p, struct linux_chown_args *args)
{
	struct chown_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(chown))
		printf(ARGS(chown, "%s, %d, %d"),
		    args->path, args->uid, args->gid);
#endif
	bsd.path = args->path;
	/* XXX size casts here */
	bsd.uid = args->uid;
	bsd.gid = args->gid;

	return chown(p, &bsd);
}

int
linux_lchown(struct proc *p, struct linux_lchown_args *args)
{
	struct lchown_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(lchown))
		printf(ARGS(lchown, "%s, %d, %d"),
		    args->path, args->uid, args->gid);
#endif
	bsd.path = args->path;
	/* XXX size casts here */
	bsd.uid = args->uid;
	bsd.gid = args->gid;

	return lchown(p, &bsd);
}

int
linux_mkdir(struct proc *p, struct linux_mkdir_args *args)
{
	struct mkdir_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTCREAT(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(mkdir))
		printf(ARGS(mkdir, "%s, %d"), args->path, args->mode);
#endif
	bsd.path = args->path;
	bsd.mode = args->mode;

	return mkdir(p, &bsd);
}

int
linux_rmdir(struct proc *p, struct linux_rmdir_args *args)
{
	struct rmdir_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(rmdir))
		printf(ARGS(rmdir, "%s"), args->path);
#endif
	bsd.path = args->path;

	return rmdir(p, &bsd);
}

int
linux_rename(struct proc *p, struct linux_rename_args *args)
{
	struct rename_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->from);
	CHECKALTCREAT(p, &sg, args->to);

#ifdef DEBUG
	if (ldebug(rename))
		printf(ARGS(rename, "%s, %s"), args->from, args->to);
#endif
	bsd.from = args->from;
	bsd.to = args->to;

	return rename(p, &bsd);
}

int
linux_symlink(struct proc *p, struct linux_symlink_args *args)
{
	struct symlink_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);
	CHECKALTCREAT(p, &sg, args->to);

#ifdef DEBUG
	if (ldebug(symlink))
		printf(ARGS(symlink, "%s, %s"), args->path, args->to);
#endif
	bsd.path = args->path;
	bsd.link = args->to;

	return symlink(p, &bsd);
}

int
linux_readlink(struct proc *p, struct linux_readlink_args *args)
{
	struct readlink_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->name);

#ifdef DEBUG
	if (ldebug(readlink))
		printf(ARGS(readlink, "%s, %p, %d"),
		    args->name, (void *)args->buf, args->count);
#endif
	bsd.path = args->name;
	bsd.buf = args->buf;
	bsd.count = args->count;

	return readlink(p, &bsd);
}

int
linux_truncate(struct proc *p, struct linux_truncate_args *args)
{
	struct truncate_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	if (ldebug(truncate))
		printf(ARGS(truncate, "%s, %ld"), args->path, args->length);
#endif
	bsd.path = args->path;
	bsd.length = args->length;

	return truncate(p, &bsd);
}

int
linux_link(struct proc *p, struct linux_link_args *args)
{
    struct link_args bsd;
    caddr_t sg;

    sg = stackgap_init();
    CHECKALTEXIST(p, &sg, args->path);
    CHECKALTCREAT(p, &sg, args->to);

#ifdef DEBUG
	if (ldebug(link))
		printf(ARGS(link, "%s, %s"), args->path, args->to);
#endif

    bsd.path = args->path;
    bsd.link = args->to;

    return link(p, &bsd);
}

int
linux_getcwd(struct proc *p, struct linux_getcwd_args *args)
{
	struct __getcwd_args bsd;
	caddr_t sg;
	int error, len;

#ifdef DEBUG
	if (ldebug(getcwd))
		printf(ARGS(getcwd, "%p, %ld"), args->buf, args->bufsize);
#endif

	sg = stackgap_init();
	bsd.buf = stackgap_alloc(&sg, SPARE_USRSPACE);
	bsd.buflen = SPARE_USRSPACE;
	error = __getcwd(p, &bsd);
	if (!error) {
		len = strlen(bsd.buf) + 1;
		if (len <= args->bufsize) {
			p->p_retval[0] = len;
			error = copyout(bsd.buf, args->buf, len);
		}
		else
			error = ERANGE;
	}
	return (error);
}

#ifndef __alpha__
int
linux_fdatasync(p, uap)
	struct proc *p;
	struct linux_fdatasync_args *uap;
{
	struct fsync_args bsd;

	bsd.fd = uap->fd;
	return fsync(p, &bsd);
}
#endif /*!__alpha__*/

int
linux_pread(p, uap)
	struct proc *p;
	struct linux_pread_args *uap;
{
	struct pread_args bsd;

	bsd.fd = uap->fd;
	bsd.buf = uap->buf;
	bsd.nbyte = uap->nbyte;
	bsd.offset = uap->offset;
	return pread(p, &bsd);
}

int
linux_pwrite(p, uap)
	struct proc *p;
	struct linux_pwrite_args *uap;
{
	struct pwrite_args bsd;

	bsd.fd = uap->fd;
	bsd.buf = uap->buf;
	bsd.nbyte = uap->nbyte;
	bsd.offset = uap->offset;
	return pwrite(p, &bsd);
}

int
linux_mount(struct proc *p, struct linux_mount_args *args)
{
	struct ufs_args ufs;
        char fstypename[MFSNAMELEN];
        char mntonname[MNAMELEN], mntfromname[MNAMELEN];
	int error;
	int fsflags;
	const char *fstype;
	void *fsdata;

        error = copyinstr(args->filesystemtype, fstypename, MFSNAMELEN - 1,
	    NULL);
	if (error)
                return (error);
        error = copyinstr(args->specialfile, mntfromname, MFSNAMELEN - 1, NULL);
	if (error)
                return (error);
        error = copyinstr(args->dir, mntonname, MFSNAMELEN - 1, NULL);
	if (error)
                return (error);

#ifdef DEBUG
	if (ldebug(mount))
		printf(ARGS(mount, "%s, %s, %s"),
		    fstypename, mntfromname, mntonname);
#endif

	if (strcmp(fstypename, "ext2") == 0) {
		fstype = "ext2fs";
		fsdata = &ufs;
		ufs.fspec = mntfromname;
#define DEFAULT_ROOTID		-2
		ufs.export.ex_root = DEFAULT_ROOTID;
		ufs.export.ex_flags =
		    args->rwflag & LINUX_MS_RDONLY ? MNT_EXRDONLY : 0;
	} else if (strcmp(fstypename, "proc") == 0) {
		fstype = "linprocfs";
		fsdata = NULL;
	} else {
		return (ENODEV);
	}

	fsflags = 0;

	if ((args->rwflag & 0xffff0000) == 0xc0ed0000) {
		/*
		 * Linux SYNC flag is not included; the closest equivalent
		 * FreeBSD has is !ASYNC, which is our default.
		 */
		if (args->rwflag & LINUX_MS_RDONLY)
			fsflags |= MNT_RDONLY; 
		if (args->rwflag & LINUX_MS_NOSUID)
			fsflags |= MNT_NOSUID; 
		if (args->rwflag & LINUX_MS_NODEV)
			fsflags |= MNT_NODEV; 
		if (args->rwflag & LINUX_MS_NOEXEC)
			fsflags |= MNT_NOEXEC; 
		if (args->rwflag & LINUX_MS_REMOUNT)
			fsflags |= MNT_UPDATE; 
	}

	return (vfs_mount(p, fstype, mntonname, fsflags, fsdata));
}

int
linux_umount(struct proc *p, struct linux_umount_args *args)
{
	struct linux_umount2_args args2;

	args2.path = args->path;
	args2.flags = 0;
	return (linux_umount2(p, &args2));
}

int
linux_umount2(struct proc *p, struct linux_umount2_args *args)
{
	struct unmount_args bsd;

	bsd.path = args->path;
	bsd.flags = args->flags;	/* XXX correct? */
	return (unmount(p, &bsd));
}
