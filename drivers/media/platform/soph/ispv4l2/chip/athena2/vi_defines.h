#ifndef __VI_DEFINES_H__
#define __VI_DEFINES_H__

#ifdef __cplusplus
	extern "C" {
#endif

#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/cvi_defines.h>
#include <linux/vi_tun_cfg.h>
#include <linux/vi_isp.h>
#include <vip/vi_drv.h>

#include <linux/videodev2.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-common.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>

enum E_VI_TH {
	E_VI_TH_PRERAW,
	E_VI_TH_ERR_HANDLER,
	E_VI_TH_EVENT_HANDLER,
	E_VI_TH_VBLANK_HANDLER,
	E_VI_TH_MAX
};

struct cvi_vip_fmt {
	u32 fourcc;
	u8  buffers;
	u32 bit_depth[3];
	u8 plane_sub_h;
	u8 plane_sub_v;
};
struct cvi_pipe_attr {
	u32 colorspace;
	u32 bytesperline[3];
	u32 sizeimage[3];
};

struct cvi_vdev_node {
	struct vb2_queue vb_q;
	struct mutex vlock;
	struct video_device vdev;
	struct media_pad pad;
};

struct vi_thread_attr {
	char th_name[32];
	struct task_struct *w_thread;
	atomic_t           thread_exit;
	wait_queue_head_t  wq;
	u32                flag;
	int (*th_handler)(void *arg);
};

/**
 * struct cvi_vi - VI IP abstraction
 */
struct cvi_vi_dev {
	struct device		*dev;
	struct class		*vi_class;
	struct cdev		cdev;
	dev_t			cdev_id;
	void __iomem		*reg_base;
	int			irq_num;
	struct clk		*clk_sys[6];
	struct clk		*clk_isp[3];
	struct clk		*clk_mac[8];
	void			*shared_mem;
	struct isp_ctx		ctx;
	struct cvi_isp_mbus_framefmt	usr_fmt;
	struct cvi_isp_rect	usr_crop;
	struct cvi_vdev_node	vnode[VI_MAX_CHN_NUM];
	struct cvi_pipe_attr	pipe[VI_MAX_CHN_NUM];
	struct cvi_vip_fmt	*fmt;
	u8			num_dev;
	u32			vid_caps;
	u64			usr_pic_phy_addr[ISP_RAW_PATH_MAX];
	unsigned long		usr_pic_delay;
	enum cvi_isp_source	isp_source;
	struct cvi_isp_snr_info	snr_info[ISP_PRERAW_MAX];
	atomic_t		isp_raw_dump_en[ISP_PRERAW_MAX];
	atomic_t		isp_smooth_raw_dump_en[ISP_PRERAW_MAX];
	u32			isp_int_flag[ISP_PRERAW_MAX];
	wait_queue_head_t	isp_int_wait_q[ISP_PRERAW_MAX];
	wait_queue_head_t	isp_dq_wait_q[VI_MAX_CHN_NUM];
	wait_queue_head_t	isp_event_wait_q;
	wait_queue_head_t	isp_dbg_wait_q;
	atomic_t		isp_dbg_flag;
	atomic_t		isp_err_handle_flag;
	enum cvi_isp_raw	offline_raw_num;
	struct tasklet_struct	job_work;
	struct list_head	qbuf_list[VI_MAX_CHN_NUM];
	struct list_head	dqbuf_list[VI_MAX_CHN_NUM];
	spinlock_t		qbuf_lock;
	u32			qbuf_num[VI_MAX_CHN_NUM];
	u32			dqbuf_num[VI_MAX_CHN_NUM];
	u32			splt_wdma_frm_num[ISP_SPLT_MAX][ISP_SPLT_CHN_MAX];
	u32			splt_rdma_frm_num[ISP_SPLT_MAX][ISP_SPLT_CHN_MAX];
	u32			pre_fe_sof_cnt[ISP_PRERAW_MAX][ISP_FE_CHN_MAX];
	u32			pre_fe_frm_num[ISP_PRERAW_MAX][ISP_FE_CHN_MAX];
	u32			pre_be_frm_num[ISP_PRERAW_MAX][ISP_BE_CHN_MAX];
	bool			preraw_first_frm[ISP_PRERAW_MAX];
	u32			postraw_frame_number[ISP_PRERAW_MAX];
	u32			drop_frame_number[ISP_PRERAW_MAX];
	u32			dump_frame_number[ISP_PRERAW_MAX];
	u8			postraw_proc_num;
	atomic_t		splt_state[ISP_SPLT_MAX][ISP_SPLT_CHN_MAX];
	atomic_t		pre_fe_state[ISP_PRERAW_MAX][ISP_FE_CHN_MAX];
	atomic_t		pre_be_state[ISP_BE_CHN_MAX];
	atomic_t		postraw_state;
	atomic_t		isp_streamoff;
	atomic_t		isp_streamon;
	atomic_t		ol_sc_frm_done;
	atomic_t		post_dq_flag[VI_MAX_CHN_NUM];
	atomic_t		is_streaming[ISP_PRERAW_MAX];
	atomic_t		file_open_cnt[VI_MAX_CHN_NUM];
	atomic_t		open_dev_cnt;
        atomic_t		isp_dump_yuv[ISP_PRERAW_MAX];
	struct mutex		vq_lock[VI_MAX_CHN_NUM];
	struct mutex		dev_lock;
        struct mutex		stream_lock;
	struct vi_thread_attr	vi_th[E_VI_TH_MAX];
};

#ifdef __cplusplus
}
#endif

#endif /* __VI_DEFINES_H__ */
