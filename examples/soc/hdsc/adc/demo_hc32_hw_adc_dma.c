/*******************************************************************************
*                                 AMetal
*                       ----------------------------
*                       innovating embedded platform
*
* Copyright (c) 2001-2018 Guangzhou ZHIYUAN Electronics Co., Ltd.
* All rights reserved.
*
* Contact information:
* web site:    http://www.zlg.cn/
*******************************************************************************/

/**
 * \file
 * \brief ADC��ͨ��ͨ��DMA���䣬ͨ��������ӿ�ʵ��
 *
 * - ʵ������
 *   1. ��ӦADCͨ����ӡ��ѹֵ��
 *
 * \par Դ����
 * \snippet demo_hc32_adc_dma.c src_hc32_adc_dma
 *
 * \internal
 * \par Modification History
 * - 1.00 19-09-25  zp, first implementation
 * \endinternal
 */

/**
 * \addtogroup demo_if_hc32_adc_dma
 * \copydoc demo_hc32_adc_dma.c
 */

/** [src_hc32_adc_dma] */
#include "ametal.h"
#include "am_delay.h"
#include "am_vdebug.h"
#include "am_hc32_dma.h"
#include "am_hc32_clk.h"
#include "hw/amhw_hc32_adc.h"
#include "hc32_periph_map.h"

#define  BUF_SIZE    16

static uint32_t __g_buf_dst[BUF_SIZE];   /**< \brief Ŀ������ݻ����� */

static uint16_t __g_adc_dat[16];         /**< \brief ADC���ݻ����� */

static am_bool_t g_trans_done;           /**< \brief ������ɱ�־ */

static int __g_adc_chan_num;             /* ADCͨ���� */

/**
 * \brief DMA �жϷ������
 */
static void dma_isr (void *p_arg , uint32_t flag)
{
    int i;

    if (flag == AM_HC32_DMA_INT_COMPLETE) {

        for (i = 0; i < __g_adc_chan_num; i++) {

            /* ��12λΪADC�ɼ����� */
            __g_adc_dat[i] = __g_buf_dst[i] & 0xfff;
        }

        /* ˳��ɨ��ת��ֹͣ */
        amhw_hc32_adc_sqr_convert_stop((amhw_hc32_adc_t *)p_arg);

        g_trans_done = AM_TRUE;
    }
}

static void __hc32_adc_init (amhw_hc32_adc_t *p_hw_adc,
                               int               *p_adc_chan,
                               int                chan_num)
{
    int i;

    /* �ڲ��ο���ѹʹ�� */
    amhw_hc32_adc_inrefvol_enable(p_hw_adc);

    /* ���òο���ѹԴ */
    amhw_hc32_adc_refvol_sel(p_hw_adc, AMHW_HC32_ADC_REFVOL_AVCC);

    /* BGRģ��ʹ�� */
    amhw_hc32_bgr_enable(AM_TRUE);

    /* ADCʹ��*/
    amhw_hc32_adc_enable(p_hw_adc);

    /* ��ʱ 20us���ȴ� ADC�� BGRģ��������� */
    am_udelay(20);

    /* ADC���� */
    amhw_hc32_adc_disable(p_hw_adc);

    /* ʹ��ɨ��ת��ģʽ */
    amhw_hc32_adc_mode_sel(p_hw_adc, AMHW_HC32_ADC_MODE_SCAN);

    amhw_hc32_adc_align_way_sel(p_hw_adc, AMHW_HC32_ADC_DATA_ALIGN_RIGHT);

    /* ����ת������ */
    amhw_hc32_adc_sample_period_sel(p_hw_adc,
                                  AMHW_HC32_ADC_SAMPLE_PERIOD_12);

    /* ����ADCʱ�ӷ�Ƶ */
    amhw_hc32_adc_pclk_div_sel(p_hw_adc, AMHW_HC32_ADC_PCLK_DIV8);

    /* ���뷽ʽ���� */
    amhw_hc32_adc_align_way_sel(p_hw_adc,
                              AMHW_HC32_ADC_DATA_ALIGN_RIGHT);

    /* ����ADC˳��ɨ��ת��ͨ�������� */
    for(i = 0; i < chan_num; i++) {
        amhw_hc32_adc_sqr_chan_sel(p_hw_adc,
                                     i,
                                     (amhw_hc32_adc_chan_sel_t)p_adc_chan[i]);
    }

    amhw_hc32_adc_sqr_num_set(p_hw_adc, chan_num - 1);

    /* ˳��ɨ��ת��DMAʹ��*/
    amhw_hc32_adc_dma_sqr_enable(p_hw_adc);

    /* ���־λ */
    amhw_hc32_adc_flag_clr(p_hw_adc, AMHW_HC32_ADC_FLAG_ALL);

    /* ADCʹ��*/
    amhw_hc32_adc_enable(p_hw_adc);
}

/**
 * \brief �������
 */
void demo_hc32_hw_adc_dma_entry (void    *p_hw_adc,
                                   int     *p_adc_chan,
                                   int      chan_num,
                                   uint32_t vref_mv,
                                   int      dma_chan,
                                   uint8_t  dma_src)
{
    uint32_t adc_mv = 0;    /* ������ѹ */
    uint32_t flags;
    int      i;
    static   amhw_hc32_dma_xfer_desc_t g_desc;  /**< \brief DMA������ */

    amhw_hc32_adc_t *p_adc = (amhw_hc32_adc_t *)p_hw_adc;

    __g_adc_chan_num = chan_num;

    am_kprintf("The ADC HW dma Demo\r\n");

    __hc32_adc_init(p_adc, p_adc_chan, chan_num);

    /* ����DMA�жϷ����� */
    am_hc32_dma_isr_connect(dma_chan, dma_isr, (void *)p_adc);

    /* DMA �������� */
    flags = AMHW_HC32_DMA_CHAN_TRANSFER_MODE_BLOCK    | /* SPIֻ�п鴫��ģʽ */
            AMHW_HC32_DMA_CHAN_SIZE_32BIT             | /* ����λ�� 32bit */

            AMHW_HC32_DMA_CHAN_SRC_ADD_INC_ENABLE     | /* Դ��ַ���� */
            AMHW_HC32_DMA_CHAN_RELOAD_SRC_ADD_ENABLE  | /* Դ��ַ����*/

            AMHW_HC32_DMA_CHAN_DST_ADD_INC_ENABLE     | /* Ŀ���ַ���� */
            AMHW_HC32_DMA_CHAN_RELOAD_DST_ADD_ENABLE  | /* Ŀ���ַ���� */

            AMHW_HC32_DMA_CHAN_RELOAD_COUNTER_ENABLE  | /* �������������� */
            AMHW_HC32_DMA_CHAN_INT_ERR_DISABLE        | /* ��������жϽ��� */
            AMHW_HC32_DMA_CHAN_INT_TX_CMP_ENABLE      | /* ��������ж�ʹ�� */
            AMHW_HC32_DMA_CHAN_CIRCULAR_MODE_ENABLE ;   /* ����ѭ��ģʽ */

    /* ����ͨ�������� */
    am_hc32_dma_xfer_desc_build(&g_desc,                        /* ͨ�������� */
                                  (uint32_t)(&(p_adc->sqrresult[0])),/* Դ�����ݻ��� */
                                  (uint32_t)(__g_buf_dst),        /* Ŀ�����ݻ��� */
                                  (uint32_t)1,                    /* �����ֽ��� */
                                  flags);                         /* �������� */

    am_hc32_dma_xfer_desc_chan_cfg(&g_desc,
                                     AMHW_HC32_DMA_PER_TO_MER, /* ���赽�ڴ� */
                                     dma_chan);

    /* Ĭ��ͨ��0��ͨ��1���ȼ�һ������ѯ��*/
    am_hc32_dma_priority_same();

    /* ���ô��������ź���Դ */
    am_hc32_dma_chan_src_set (dma_chan, dma_src);

    /* ����ÿ��DMA������СΪ�ѿ�����ͨ��������chan_num�� */
    am_hc32_dma_block_data_size(dma_chan, chan_num);

    /* ��ʼͨ������ */
    am_hc32_dma_chan_start(dma_chan);

    while(1) {

        /* ˳��ɨ��ת������ */
        amhw_hc32_adc_sqr_convert_start(p_adc);

        while(g_trans_done == AM_FALSE); /* �ȴ�������� */

        for (i = 0; i < chan_num; i++) {

            /* ת��Ϊ��ѹֵ��Ӧ������ֵ */
            adc_mv = __g_adc_dat[p_adc_chan[i]] * vref_mv / ((1UL << 12) -1);

            am_kprintf("chan: %d, Sample : %d, Vol: %d mv\r\n", i,
                                                                __g_adc_dat[i],
                                                                adc_mv);
        }

        am_kprintf("\r\n");
        am_mdelay(500);

        g_trans_done = AM_FALSE;
    }
}
/** [src_hc32_adc_dma] */

/* end of file */