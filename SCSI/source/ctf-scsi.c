#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "sysemu/dma.h"
#include "sysemu/block-backend.h"
#include "hw/pci/msi.h"
#include "qemu/iov.h"
#include "hw/scsi/scsi.h"
#include "qemu/osdep.h"
#include "hw/hw.h"

#define DEVICE_NAME "ctf-scsi"
#define CTF_DEV_STATE_READY 1
    
    typedef struct 
    {
        uint8_t target_id;
        uint8_t target_bus;
        uint8_t lun;
        uint8_t pad;
        unsigned int buf_len;
        int type;

    }CTF_req_head;

    typedef struct 
    {
        CTF_req_head head;
        char *cmd_buf;
    } CTF_req; 

    typedef struct {
        PCIDevice pdev;
        MemoryRegion mmio;
        SCSIBus bus;
        uint64_t high_addr;
        int state;
        int register_a;
        int register_b;
        int register_c;
        int pwidx;
        char pw[4];
        SCSIRequest * cur_req;
        int (*dma_read)(void *,char *,int);
        int (*dma_write)(void *,char *,int);
        CTF_req req;
        char *dma_buf;
        int dma_buf_len;
        int dma_need;
    } CTFState;


    static int ctf_dma_read(void *opaque,char *buf,int len)
    {
        CTFState *s = opaque;
        if(s->state&(1<<3)){
            s->state |=1<<2;
            s->dma_need=len;
            return 0;
        }
        if(s->dma_buf&&s->dma_buf_len)
        {
            if(s->dma_buf_len>=len&&s->state&(1<<5))
            {
                memcpy(buf,s->dma_buf,len);
                free(s->dma_buf);
                s->dma_buf=0;
                s->dma_buf_len=0;
                s->dma_need=0;
                return 1;
            }
            else
            {
                s->dma_need=len-s->dma_buf_len;
                s->state |=1<<2;
                return 0;
            }
        }
        s->state |=1<<2;
        s->dma_need=len;
        return 0;
    }

    static int ctf_dma_write(void *opaque,char *buf,int len)
    {
        CTFState *s = opaque;
        if(!s->dma_buf)
        {
            s->dma_buf=malloc(len);
            if(!s->dma_buf)
                return 0;
            s->dma_buf_len=len;
            memcpy(s->dma_buf,buf,len);
            s->state |=1<<3;
            return 1;
        }
        s->state |=1<<6;
        s->dma_need=len;
        return 0;
    }

    static void ctf_transfer_data(SCSIRequest *req,uint32_t len)
    {
        CTFState *s = req->hba_private;
        char *buf;
        int ret=0;
        buf = scsi_req_get_buf(req);
        if(!req)
            return;
        if(req->cmd.mode==2)
            ret=(*s->dma_read)(s,buf,len);
        else if(req->cmd.mode==1)
            ret=(*s->dma_write)(s,buf,len);
        if(ret>0)
            scsi_req_continue(req);
    }

    static void ctf_request_complete(SCSIRequest *req)
    {
        CTFState *s = req->hba_private;
        s->state ^=1<<4;
        free(s->req.cmd_buf);
        s->req.cmd_buf=0;
        scsi_req_unref(req);
        s->cur_req=0;
        return;
    }

    static void ctf_request_cancelled(SCSIRequest *req)
    {
        CTFState *s = req->hba_private;
        s->state ^=1<<4;
        free(s->req.cmd_buf);
        s->req.cmd_buf=0;
        scsi_req_unref(req);
        return;
    }


    static void ctf_set_io(void *opaque,uint64_t val)
    {
        CTFState *state = opaque;
        if(state->state&1)
        {
            state->high_addr = val;
            state->state &=0xfffffffe;
            state->state |=2;
        }
        return;
    }

    static void ctf_reset(void *opaque)
    {
        CTFState *s = opaque;
        s->state=0;
        s->register_a=0;
        s->register_b=0;
        s->register_c=0;
        s->pwidx=0;
        s->dma_buf=0;
        s->dma_buf_len=0;
        memset(&s->req,0,sizeof(CTF_req_head));
        s->req.cmd_buf=0;
        s->cur_req=0;
        s->high_addr=0;
        s->dma_need=0;
    }
    static void ctf_process_req(void *opaque,uint64_t val)
    {
        CTFState *state = opaque;
        CTF_req_head tmp;
        SCSIDevice *sdev;
        int len;
        uint64_t addr;
        if(state->state&2)
        {
            addr = (state->high_addr<<32) | val;
            if(state->cur_req)
                scsi_req_cancel(state->cur_req);
            cpu_physical_memory_read(addr,&tmp,sizeof(CTF_req_head));
            sdev = scsi_device_find(&state->bus,tmp.target_bus,tmp.target_id,tmp.lun);
            if(!sdev)
                return;
            state->state |=1<<4;
            state->req.head=tmp;
            state->req.cmd_buf = malloc(tmp.buf_len);
            cpu_physical_memory_read(addr+sizeof(CTF_req_head),state->req.cmd_buf,tmp.buf_len);
            state->cur_req=scsi_req_new(sdev,0,tmp.lun,state->req.cmd_buf,state);
            if(!state->cur_req)
                return;
            len = scsi_req_enqueue(state->cur_req);
            if(len!=0)
            {
                scsi_req_continue(state->cur_req);
            }
        }
    }

    static void ctf_process_reply(void *opaque)
    {
        CTFState *state=opaque;
        uint64_t addr = (state->register_a<<32)|state->register_b;
        if(state->state&(1<<3)&&addr&&state->dma_buf)
        {
            cpu_physical_memory_write(addr,state->dma_buf,state->dma_buf_len);
            state->state ^=(1<<3);
            free(state->dma_buf);
            state->dma_buf=0;
            state->dma_buf_len=0;
            return;
        }
        if(state->state&(1<<6)){
            ctf_transfer_data(state->cur_req,state->dma_need);
            state->state ^=(1<<6);
        }

    }

    void ctf_add_cmd_data(void *opaque,uint64_t val)
    {
        CTFState *state=opaque;
        uint64_t addr = (state->register_a<<32)|state->register_b;
        if(state->state&(1<<3))
            return;
        if(!state->dma_buf)
        {
            state->dma_buf=malloc(val);
            if(!state->dma_buf)
                return;
            cpu_physical_memory_read(addr,state->dma_buf,val);
            state->dma_buf_len=val;
            state->state |=1<<5;
        }
        else if(state->state&4 && state->dma_need && (state->dma_buf_len+val>=state->dma_need))
        {   
            state->dma_buf=realloc(state->dma_buf,state->dma_buf_len+state->dma_need);
            cpu_physical_memory_read(addr,state->dma_buf+state->dma_buf_len,state->dma_need);
            state->dma_buf_len += state->dma_need;
            ctf_transfer_data(state->cur_req,state->dma_buf_len);
            state->state ^=4;
        }
    }

    static uint64_t ctf_mmio_read(void *opaque, hwaddr addr, unsigned size)
    {
        CTFState *state = opaque;
        switch (addr)
        {
            case 0x0:
                return state->state;
            case 0x4:
                return state->high_addr;
            case 0x8:
                return state->register_a;
            case 0xc:
                return state->register_b;
            case 0x10:
                return state->register_c;
            case 0x14:
                return state->pwidx;
            case 0x18:
                return state->dma_need;
            case 0x1c:
                return state->dma_buf_len;

        }
        return 0;
    }

    static void ctf_mmio_write(void *opaque, hwaddr addr, uint64_t val,
            unsigned size)
    {
        CTFState *state = opaque;
        int idx;
        switch (addr)
        {
            case 0x0:
                ctf_set_io(opaque,val);
                break;
            case 0x4:
                idx=state->pwidx;
                if(((char)(state->pw[idx]))==(char)(val&0xff)){
                    state->pwidx++;
                    if(state->pwidx==4)
                        state->state |=1;
                }
                else
                    state->pwidx=0;
                break;
            case 0x8:
                ctf_process_req(opaque,val&0xffffffff);
                break;
            case 0xc:
                ctf_reset(opaque);
                break;
            case 0x10:
                state->register_a=val&0xffffffff;
                break;
            case 0x14:
                state->register_b=val&0xffffffff;
                break;
            case 0x18:
                ctf_process_reply(opaque);
                break;
            case 0x1c:
                ctf_add_cmd_data(opaque,val);
                break;
        }
    }

    static const MemoryRegionOps mmio_ops = {
        .read = ctf_mmio_read,
        .write = ctf_mmio_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
        .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
        }
    };

    static const struct SCSIBusInfo ctf_scsi_info = {
    .tcq = true,
    .max_target = 1,
    .max_lun = 1,
    .transfer_data = ctf_transfer_data,
    .complete = ctf_request_complete,
    .cancel = ctf_request_cancelled,
    };
    static void ctf_realize(PCIDevice *pdev, Error **errp)
    {

        CTFState *s = DO_UPCAST(CTFState, pdev, pdev);
        s->pdev = *pdev;
        s->state=0;
        s->register_a=0;
        s->register_b=0;
        s->register_c=0;
        s->pwidx=0;
        s->dma_buf=0;
        s->dma_buf_len=0;
        memset(&s->req,0,sizeof(CTF_req_head));
        s->pw[0]='B';
        s->pw[1]='L';
        s->pw[2]='U';
        s->pw[3]='E';
        s->req.cmd_buf=0;
        s->cur_req=0;
        s->high_addr=0;
        s->dma_need=0;
        s->dma_write=ctf_dma_write;
        s->dma_read=ctf_dma_read;
        pci_config_set_interrupt_pin(pdev->config, 1);
        memory_region_init_io(&s->mmio, OBJECT(s), &mmio_ops, s,
                DEVICE_NAME, 0x1000);
        pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);
        scsi_bus_new(&s->bus, sizeof(s->bus), &pdev->qdev, &ctf_scsi_info, NULL);
           
    }

    static void ctf_class_init(ObjectClass *class, void *data)
    {
        PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

        k->realize = ctf_realize;
        k->vendor_id = PCI_VENDOR_ID_QEMU;
        k->device_id = 0x11e9;
        k->revision = 0x0;
        k->class_id = PCI_CLASS_OTHERS;
    }

    static const TypeInfo type_info = {
        .name          = DEVICE_NAME,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(CTFState),
        .class_init    = ctf_class_init,
    };

    static void register_types(void)
    {
        type_register_static(&type_info);
    }

    type_init(register_types)