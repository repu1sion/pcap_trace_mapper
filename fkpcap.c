#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <pcap/pcap.h>
#include "dlt.h"
#include <libtrace.h>

#define DEBUG

#define LINKTYPE_ETHERNET 	1
#define MAXIMUM_SNAPLEN		262144

#define PCAP_IF_LOOPBACK        0x00000001      /* interface is loopback */
#define PCAP_IF_UP              0x00000002      /* interface is up */
#define PCAP_IF_RUNNING         0x00000004      /* interface is running */


#define ISLOOPBACK(name, flags) ((flags) & IFF_LOOPBACK) 
#define ISUP(flags) ((flags) & IFF_UP)
#define ISRUNNING(flags) ((flags) & IFF_RUNNING)


#ifdef DEBUG
 #define debug(x...) printf(x)
#else
 #define debug(x...)
#endif

#define strlcpy(x, y, z) \
        (strncpy((x), (y), (z)), \
         ((z) <= 0 ? 0 : ((x)[(z) - 1] = '\0')), \
         strlen((y)))

#define DLT_CHOICE(code, description) { #code, description, DLT_ ## code }
#define DLT_CHOICE_SENTINEL { NULL, NULL, 0 }

struct pcap
{
	char name[30];
	int activated;
	int linktype;
	int snapshot;
	int fd;
	libtrace_t *trace;
	libtrace_packet_t *packet;
	libtrace_out_t *trace_out;
};


struct dlt_choice {
        const char *name;
        const char *description;
        int     dlt;
};

static struct dlt_choice dlt_choices[] = {
        DLT_CHOICE(NULL, "BSD loopback"),
        DLT_CHOICE(EN10MB, "Ethernet"),
        DLT_CHOICE(IEEE802, "Token ring"),
        DLT_CHOICE(ARCNET, "BSD ARCNET"),
        DLT_CHOICE(SLIP, "SLIP"),
        DLT_CHOICE(PPP, "PPP"),
        DLT_CHOICE(FDDI, "FDDI"),
        DLT_CHOICE(ATM_RFC1483, "RFC 1483 LLC-encapsulated ATM"),
        DLT_CHOICE(RAW, "Raw IP"),
        DLT_CHOICE(SLIP_BSDOS, "BSD/OS SLIP"),
        DLT_CHOICE(PPP_BSDOS, "BSD/OS PPP"),
        DLT_CHOICE(ATM_CLIP, "Linux Classical IP-over-ATM"),
        DLT_CHOICE(PPP_SERIAL, "PPP over serial"),
        DLT_CHOICE(PPP_ETHER, "PPPoE"),
        DLT_CHOICE(SYMANTEC_FIREWALL, "Symantec Firewall"),
        DLT_CHOICE(C_HDLC, "Cisco HDLC"),
        DLT_CHOICE(IEEE802_11, "802.11"),
        DLT_CHOICE(FRELAY, "Frame Relay"),
        DLT_CHOICE(LOOP, "OpenBSD loopback"),
        DLT_CHOICE(ENC, "OpenBSD encapsulated IP"),
        DLT_CHOICE(LINUX_SLL, "Linux cooked"),
        DLT_CHOICE(LTALK, "Localtalk"),
        DLT_CHOICE(PFLOG, "OpenBSD pflog file"),
        DLT_CHOICE(PFSYNC, "Packet filter state syncing"),
        DLT_CHOICE(PRISM_HEADER, "802.11 plus Prism header"),
        DLT_CHOICE(IP_OVER_FC, "RFC 2625 IP-over-Fibre Channel"),
        DLT_CHOICE(SUNATM, "Sun raw ATM"),
        DLT_CHOICE(IEEE802_11_RADIO, "802.11 plus radiotap header"),
        DLT_CHOICE(ARCNET_LINUX, "Linux ARCNET"),
        DLT_CHOICE(JUNIPER_MLPPP, "Juniper Multi-Link PPP"),
        DLT_CHOICE(JUNIPER_MLFR, "Juniper Multi-Link Frame Relay"),
        DLT_CHOICE(JUNIPER_ES, "Juniper Encryption Services PIC"),
        DLT_CHOICE(JUNIPER_GGSN, "Juniper GGSN PIC"),
        DLT_CHOICE(JUNIPER_MFR, "Juniper FRF.16 Frame Relay"),
        DLT_CHOICE(JUNIPER_ATM2, "Juniper ATM2 PIC"),
        DLT_CHOICE(JUNIPER_SERVICES, "Juniper Advanced Services PIC"),
        DLT_CHOICE(JUNIPER_ATM1, "Juniper ATM1 PIC"),
        DLT_CHOICE(APPLE_IP_OVER_IEEE1394, "Apple IP-over-IEEE 1394"),
        DLT_CHOICE(MTP2_WITH_PHDR, "SS7 MTP2 with Pseudo-header"),
        DLT_CHOICE(MTP2, "SS7 MTP2"),
        DLT_CHOICE(MTP3, "SS7 MTP3"),
        DLT_CHOICE(SCCP, "SS7 SCCP"),
        DLT_CHOICE(DOCSIS, "DOCSIS"),
        DLT_CHOICE(LINUX_IRDA, "Linux IrDA"),
        DLT_CHOICE(IEEE802_11_RADIO_AVS, "802.11 plus AVS radio information header"),
        DLT_CHOICE(JUNIPER_MONITOR, "Juniper Passive Monitor PIC"),
        DLT_CHOICE(BACNET_MS_TP, "BACnet MS/TP"),
        DLT_CHOICE(PPP_PPPD, "PPP for pppd, with direction flag"),
        DLT_CHOICE(JUNIPER_PPPOE, "Juniper PPPoE"),
        DLT_CHOICE(JUNIPER_PPPOE_ATM, "Juniper PPPoE/ATM"),
        DLT_CHOICE(GPRS_LLC, "GPRS LLC"),
        DLT_CHOICE(GPF_T, "GPF-T"),
        DLT_CHOICE(GPF_F, "GPF-F"),
        DLT_CHOICE(JUNIPER_PIC_PEER, "Juniper PIC Peer"),
        DLT_CHOICE(ERF_ETH,     "Ethernet with Endace ERF header"),
        DLT_CHOICE(ERF_POS, "Packet-over-SONET with Endace ERF header"),
        DLT_CHOICE(LINUX_LAPD, "Linux vISDN LAPD"),
        DLT_CHOICE(JUNIPER_ETHER, "Juniper Ethernet"),
        DLT_CHOICE(JUNIPER_PPP, "Juniper PPP"),
        DLT_CHOICE(JUNIPER_FRELAY, "Juniper Frame Relay"),
        DLT_CHOICE(JUNIPER_CHDLC, "Juniper C-HDLC"),
        DLT_CHOICE(MFR, "FRF.16 Frame Relay"),
        DLT_CHOICE(JUNIPER_VP, "Juniper Voice PIC"),
        DLT_CHOICE(A429, "Arinc 429"),
        DLT_CHOICE(A653_ICM, "Arinc 653 Interpartition Communication"),
        DLT_CHOICE(USB_FREEBSD, "USB with FreeBSD header"),
        DLT_CHOICE(BLUETOOTH_HCI_H4, "Bluetooth HCI UART transport layer"),
        DLT_CHOICE(IEEE802_16_MAC_CPS, "IEEE 802.16 MAC Common Part Sublayer"),
        DLT_CHOICE(USB_LINUX, "USB with Linux header"),
        DLT_CHOICE(CAN20B, "Controller Area Network (CAN) v. 2.0B"),
        DLT_CHOICE(IEEE802_15_4_LINUX, "IEEE 802.15.4 with Linux padding"),
        DLT_CHOICE(PPI, "Per-Packet Information"),
        DLT_CHOICE(IEEE802_16_MAC_CPS_RADIO, "IEEE 802.16 MAC Common Part Sublayer plus radiotap header"),
        DLT_CHOICE(JUNIPER_ISM, "Juniper Integrated Service Module"),
        DLT_CHOICE(IEEE802_15_4, "IEEE 802.15.4 with FCS"),
        DLT_CHOICE(SITA, "SITA pseudo-header"),
        DLT_CHOICE(ERF, "Endace ERF header"),
        DLT_CHOICE(RAIF1, "Ethernet with u10 Networks pseudo-header"),
        DLT_CHOICE(IPMB, "IPMB"),
        DLT_CHOICE(JUNIPER_ST, "Juniper Secure Tunnel"),
        DLT_CHOICE(BLUETOOTH_HCI_H4_WITH_PHDR, "Bluetooth HCI UART transport layer plus pseudo-header"),
        DLT_CHOICE(AX25_KISS, "AX.25 with KISS header"),
        DLT_CHOICE(IEEE802_15_4_NONASK_PHY, "IEEE 802.15.4 with non-ASK PHY data"),
        DLT_CHOICE(MPLS, "MPLS with label as link-layer header"),
        DLT_CHOICE(LINUX_EVDEV, "Linux evdev events"),
        DLT_CHOICE(USB_LINUX_MMAPPED, "USB with padded Linux header"),
        DLT_CHOICE(DECT, "DECT"),
        DLT_CHOICE(AOS, "AOS Space Data Link protocol"),
        DLT_CHOICE(WIHART, "Wireless HART"),
        DLT_CHOICE(FC_2, "Fibre Channel FC-2"),
        DLT_CHOICE(FC_2_WITH_FRAME_DELIMS, "Fibre Channel FC-2 with frame delimiters"),
        DLT_CHOICE(IPNET, "Solaris ipnet"),
        DLT_CHOICE(CAN_SOCKETCAN, "CAN-bus with SocketCAN headers"),
        DLT_CHOICE(IPV4, "Raw IPv4"),
        DLT_CHOICE(IPV6, "Raw IPv6"),
        DLT_CHOICE(IEEE802_15_4_NOFCS, "IEEE 802.15.4 without FCS"),
        DLT_CHOICE(DBUS, "D-Bus"),
        DLT_CHOICE(JUNIPER_VS, "Juniper Virtual Server"),
        DLT_CHOICE(JUNIPER_SRX_E2E, "Juniper SRX E2E"),
        DLT_CHOICE(JUNIPER_FIBRECHANNEL, "Juniper Fibre Channel"),
        DLT_CHOICE(DVB_CI, "DVB-CI"),
        DLT_CHOICE(MUX27010, "MUX27010"),
        DLT_CHOICE(STANAG_5066_D_PDU, "STANAG 5066 D_PDUs"),
        DLT_CHOICE(JUNIPER_ATM_CEMIC, "Juniper ATM CEMIC"),
        DLT_CHOICE(NFLOG, "Linux netfilter log messages"),
        DLT_CHOICE(NETANALYZER, "Ethernet with Hilscher netANALYZER pseudo-header"),
       	DLT_CHOICE(NETANALYZER_TRANSPARENT, "Ethernet with Hilscher netANALYZER pseudo-header and with preamble and SFD"),
        DLT_CHOICE(IPOIB, "RFC 4391 IP-over-Infiniband"),
        DLT_CHOICE(MPEG_2_TS, "MPEG-2 transport stream"),
        DLT_CHOICE(NG40, "ng40 protocol tester Iub/Iur"),
        DLT_CHOICE(NFC_LLCP, "NFC LLCP PDUs with pseudo-header"),
        DLT_CHOICE(INFINIBAND, "InfiniBand"),
        DLT_CHOICE(SCTP, "SCTP"),
        DLT_CHOICE(USBPCAP, "USB with USBPcap header"),
        DLT_CHOICE(RTAC_SERIAL, "Schweitzer Engineering Laboratories RTAC packets"),
        DLT_CHOICE(BLUETOOTH_LE_LL, "Bluetooth Low Energy air interface"),
        DLT_CHOICE(NETLINK, "Linux netlink"),
        DLT_CHOICE(BLUETOOTH_LINUX_MONITOR, "Bluetooth Linux Monitor"),
        DLT_CHOICE(BLUETOOTH_BREDR_BB, "Bluetooth Basic Rate/Enhanced Data Rate baseband packets"),
        DLT_CHOICE(BLUETOOTH_LE_LL_WITH_PHDR, "Bluetooth Low Energy air interface with pseudo-header"),
        DLT_CHOICE(PROFIBUS_DL, "PROFIBUS data link layer"),
        DLT_CHOICE(PKTAP, "Apple DLT_PKTAP"),
        DLT_CHOICE(EPON, "Ethernet with 802.3 Clause 65 EPON preamble"),
        DLT_CHOICE(IPMI_HPM_2, "IPMI trace packets"),
        DLT_CHOICE(ZWAVE_R1_R2, "Z-Wave RF profile R1 and R2 packets"),
        DLT_CHOICE(ZWAVE_R3, "Z-Wave RF profile R3 packets"),
        DLT_CHOICE(WATTSTOPPER_DLM, "WattStopper Digital Lighting Management (DLM) and Legrand Nitoo Open protocol"),
        DLT_CHOICE(ISO_14443, "ISO 14443 messages"),
        DLT_CHOICE(RDS, "IEC 62106 Radio Data System groups"),
        DLT_CHOICE_SENTINEL
};

#if 0
//struct from libpcap:
struct pcap_if {
        struct pcap_if *next;
        char *name;             /* name to hand to "pcap_open_live()" */
        char *description;      /* textual description of interface, or NULL */
        struct pcap_addr *addresses;
        bpf_u_int32 flags;      /* PCAP_IF_ interface flags */
};
#endif


static u_int get_figure_of_merit(pcap_if_t *dev)
{
        const char *cp;
        u_int n;

        if (strcmp(dev->name, "any") == 0) {
                /*
                 * Give the "any" device an artificially high instance
                 * number, so it shows up after all other non-loopback
                 * interfaces.
                 */
                n = 0x1FFFFFFF; /* 29 all-1 bits */
        } else {
                /*
                 * A number at the end of the device name string is
                 * assumed to be a unit number.
                 */
                cp = dev->name + strlen(dev->name) - 1;
                while (cp-1 >= dev->name && *(cp-1) >= '0' && *(cp-1) <= '9')
                        cp--;
                if (*cp >= '0' && *cp <= '9')
                        n = atoi(cp);
                else
                        n = 0;
        }
        if (!(dev->flags & PCAP_IF_RUNNING))
                n |= 0x80000000;
        if (!(dev->flags & PCAP_IF_UP))
                n |= 0x40000000;
        if (dev->flags & PCAP_IF_LOOPBACK)
                n |= 0x20000000;
        return (n);
}

static int add_linux_if(pcap_if_t **devlistp, const char *ifname, int fd, char *errbuf)
{
	const char *p;
	char name[512];	/* XXX - pick a size */
	char *q, *saveq;
	struct ifreq ifrflags;

	/*
	 * Get the interface name.
	 */
	p = ifname;
	q = &name[0];
	while (*p != '\0' && isascii(*p) && !isspace(*p)) {
		if (*p == ':') {
			/*
			 * This could be the separator between a
			 * name and an alias number, or it could be
			 * the separator between a name with no
			 * alias number and the next field.
			 *
			 * If there's a colon after digits, it
			 * separates the name and the alias number,
			 * otherwise it separates the name and the
			 * next field.
			 */
			saveq = q;
			while (isascii(*p) && isdigit(*p))
				*q++ = *p++;
			if (*p != ':') {
				/*
				 * That was the next field,
				 * not the alias number.
				 */
				q = saveq;
			}
			break;
		} else
			*q++ = *p++;
	}
	*q = '\0';

	/*
	 * Get the flags for this interface.
	 */
	strlcpy(ifrflags.ifr_name, name, sizeof(ifrflags.ifr_name));
	if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifrflags) < 0) {
		if (errno == ENXIO || errno == ENODEV)
			return (0);	/* device doesn't actually exist - ignore it */
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "SIOCGIFFLAGS: %.*s: %s",
		    (int)sizeof(ifrflags.ifr_name),
		    ifrflags.ifr_name,
		    pcap_strerror(errno));
		return (-1);
	}

	/*
	 * Add an entry for this interface, with no addresses.
	 */
	if (pcap_add_if(devlistp, name, ifrflags.ifr_flags, NULL,
	    errbuf) == -1) {
		/*
		 * Failure.
		 */
		return (-1);
	}

	return (0);
}

static int add_or_find_if(pcap_if_t **curdev_ret, pcap_if_t **alldevs, const char *name,
    u_int flags, const char *description, char *errbuf)
{
	pcap_t *p;
	pcap_if_t *curdev, *prevdev, *nextdev;
	u_int this_figure_of_merit, nextdev_figure_of_merit;
	char open_errbuf[PCAP_ERRBUF_SIZE];
	int ret;

	/*
	 * Is there already an entry in the list for this interface?
	 */
	for (curdev = *alldevs; curdev != NULL; curdev = curdev->next) {
		if (strcmp(name, curdev->name) == 0)
			break;	/* yes, we found it */
	}

	if (curdev == NULL) {
		/*
		 * No, we didn't find it.
		 *
		 * Can we open this interface for live capture?
		 *
		 * We do this check so that interfaces that are
		 * supplied by the interface enumeration mechanism
		 * we're using but that don't support packet capture
		 * aren't included in the list.  Loopback interfaces
		 * on Solaris are an example of this; we don't just
		 * omit loopback interfaces on all platforms because
		 * you *can* capture on loopback interfaces on some
		 * OSes.
		 *
		 * On OS X, we don't do this check if the device
		 * name begins with "wlt"; at least some versions
		 * of OS X offer monitor mode capturing by having
		 * a separate "monitor mode" device for each wireless
		 * adapter, rather than by implementing the ioctls
		 * that {Free,Net,Open,DragonFly}BSD provide.
		 * Opening that device puts the adapter into monitor
		 * mode, which, at least for some adapters, causes
		 * them to deassociate from the network with which
		 * they're associated.
		 *
		 * Instead, we try to open the corresponding "en"
		 * device (so that we don't end up with, for users
		 * without sufficient privilege to open capture
		 * devices, a list of adapters that only includes
		 * the wlt devices).
		 */
		p = pcap_create(name, open_errbuf);
		if (p == NULL) {
			/*
			 * The attempt to create the pcap_t failed;
			 * that's probably an indication that we're
			 * out of memory.
			 *
			 * Don't bother including this interface,
			 * but don't treat it as an error.
			 */
			*curdev_ret = NULL;
			return (0);
		}
		/* Small snaplen, so we don't try to allocate much memory. */
		pcap_set_snaplen(p, 68);
		ret = pcap_activate(p);
		pcap_close(p);
		switch (ret) {

		case PCAP_ERROR_NO_SUCH_DEVICE:
		case PCAP_ERROR_IFACE_NOT_UP:
			/*
			 * We expect these two errors - they're the
			 * reason we try to open the device.
			 *
			 * PCAP_ERROR_NO_SUCH_DEVICE typically means
			 * "there's no such device *known to the
			 * OS's capture mechanism*", so, even though
			 * it might be a valid network interface, you
			 * can't capture on it (e.g., the loopback
			 * device in Solaris up to Solaris 10, or
			 * the vmnet devices in OS X with VMware
			 * Fusion).  We don't include those devices
			 * in our list of devices, as there's no
			 * point in doing so - they're not available
			 * for capture.
			 *
			 * PCAP_ERROR_IFACE_NOT_UP means that the
			 * OS's capture mechanism doesn't work on
			 * interfaces not marked as up; some capture
			 * mechanisms *do* support that, so we no
			 * longer reject those interfaces out of hand,
			 * but we *do* want to reject them if they
			 * can't be opened for capture.
			 */
			*curdev_ret = NULL;
			return (0);
		}

		/*
		 * Yes, we can open it, or we can't, for some other
		 * reason.
		 *
		 * If we can open it, we want to offer it for
		 * capture, as you can capture on it.  If we can't,
		 * we want to offer it for capture, so that, if
		 * the user tries to capture on it, they'll get
		 * an error and they'll know why they can't
		 * capture on it (e.g., insufficient permissions)
		 * or they'll report it as a problem (and then
		 * have the error message to provide as information).
		 *
		 * Allocate a new entry.
		 */
		curdev = malloc(sizeof(pcap_if_t));
		if (curdev == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			return (-1);
		}

		/*
		 * Fill in the entry.
		 */
		curdev->next = NULL;
		curdev->name = strdup(name);
		if (curdev->name == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curdev);
			return (-1);
		}
		if (description != NULL) {
			/*
			 * We have a description for this interface.
			 */
			curdev->description = strdup(description);
			if (curdev->description == NULL) {
				(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "malloc: %s", pcap_strerror(errno));
				free(curdev->name);
				free(curdev);
				return (-1);
			}
		} else {
			/*
			 * We don't.
			 */
			curdev->description = NULL;
		}
		curdev->addresses = NULL;	/* list starts out as empty */
		curdev->flags = 0;
		if (ISLOOPBACK(name, flags))
			curdev->flags |= PCAP_IF_LOOPBACK;
		if (ISUP(flags))
			curdev->flags |= PCAP_IF_UP;
		if (ISRUNNING(flags))
			curdev->flags |= PCAP_IF_RUNNING;

		/*
		 * Add it to the list, in the appropriate location.
		 * First, get the "figure of merit" for this
		 * interface.
		 */
		this_figure_of_merit = get_figure_of_merit(curdev);

		/*
		 * Now look for the last interface with an figure of merit
		 * less than or equal to the new interface's figure of
		 * merit.
		 *
		 * We start with "prevdev" being NULL, meaning we're before
		 * the first element in the list.
		 */
		prevdev = NULL;
		for (;;) {
			/*
			 * Get the interface after this one.
			 */
			if (prevdev == NULL) {
				/*
				 * The next element is the first element.
				 */
				nextdev = *alldevs;
			} else
				nextdev = prevdev->next;

			/*
			 * Are we at the end of the list?
			 */
			if (nextdev == NULL) {
				/*
				 * Yes - we have to put the new entry
				 * after "prevdev".
				 */
				break;
			}

			/*
			 * Is the new interface's figure of merit less
			 * than the next interface's figure of merit,
			 * meaning that the new interface is better
			 * than the next interface?
			 */
			nextdev_figure_of_merit = get_figure_of_merit(nextdev);
			if (this_figure_of_merit < nextdev_figure_of_merit) {
				/*
				 * Yes - we should put the new entry
				 * before "nextdev", i.e. after "prevdev".
				 */
				break;
			}

			prevdev = nextdev;
		}

		/*
		 * Insert before "nextdev".
		 */
		curdev->next = nextdev;

		/*
		 * Insert after "prevdev" - unless "prevdev" is null,
		 * in which case this is the first interface.
		 */
		if (prevdev == NULL) {
			/*
			 * This is the first interface.  Pass back a
			 * pointer to it, and put "curdev" before
			 * "nextdev".
			 */
			*alldevs = curdev;
		} else
			prevdev->next = curdev;
	}

	*curdev_ret = curdev;
	return (0);
}

int pcap_add_if(pcap_if_t **devlist, const char *name, u_int flags,
    const char *description, char *errbuf)
{
        pcap_if_t *curdev;

        return (add_or_find_if(&curdev, devlist, name, flags, description,
            errbuf));
}

static int scan_sys_class_net(pcap_if_t **devlistp, char *errbuf)
{
	DIR *sys_class_net_d;
	int fd;
	struct dirent *ent;
	char subsystem_path[PATH_MAX+1];
	struct stat statb;
	int ret = 1;

	sys_class_net_d = opendir("/sys/class/net");
	if (sys_class_net_d == NULL) {
		/*
		 * Don't fail if it doesn't exist at all.
		 */
		if (errno == ENOENT)
			return (0);

		/*
		 * Fail if we got some other error.
		 */
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "Can't open /sys/class/net: %s", pcap_strerror(errno));
		return (-1);
	}

	/*
	 * Create a socket from which to fetch interface information.
	 */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		(void)closedir(sys_class_net_d);
		return (-1);
	}

	for (;;) {
		errno = 0;
		ent = readdir(sys_class_net_d);
		if (ent == NULL) {
			/*
			 * Error or EOF; if errno != 0, it's an error.
			 */
			break;
		}

		/*
		 * Ignore "." and "..".
		 */
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;

		/*
		 * Ignore plain files; they do not have subdirectories
		 * and thus have no attributes.
		 */
		if (ent->d_type == DT_REG)
			continue;

		/*
		 * Is there an "ifindex" file under that name?
		 * (We don't care whether it's a directory or
		 * a symlink; older kernels have directories
		 * for devices, newer kernels have symlinks to
		 * directories.)
		 */
		snprintf(subsystem_path, sizeof subsystem_path,
		    "/sys/class/net/%s/ifindex", ent->d_name);
		if (lstat(subsystem_path, &statb) != 0) {
			/*
			 * Stat failed.  Either there was an error
			 * other than ENOENT, and we don't know if
			 * this is an interface, or it's ENOENT,
			 * and either some part of "/sys/class/net/{if}"
			 * disappeared, in which case it probably means
			 * the interface disappeared, or there's no
			 * "ifindex" file, which means it's not a
			 * network interface.
			 */
			continue;
		}

		/*
		 * Attempt to add the interface.
		 */
		if (add_linux_if(devlistp, &ent->d_name[0], fd, errbuf) == -1) {
			/* Fail. */
			ret = -1;
			break;
		}
	}
	if (ret != -1) {
		/*
		 * Well, we didn't fail for any other reason; did we
		 * fail due to an error reading the directory?
		 */
		if (errno != 0) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "Error reading /sys/class/net: %s",
			    pcap_strerror(errno));
			ret = -1;
		}
	}

	(void)close(fd);
	(void)closedir(sys_class_net_d);
	return (ret);
}

int pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf)
{
	debug("[%s() start]\n", __func__);

	int rv = 0;

	//just in case we would find nothing
	*alldevsp = NULL;

	rv = scan_sys_class_net(alldevsp, errbuf);

	return rv;
}

/*
 * Free a list of interfaces.
 */
void
pcap_freealldevs(pcap_if_t *alldevs)
{
	debug("[%s() start]\n", __func__);

        pcap_if_t *curdev, *nextdev;
        pcap_addr_t *curaddr, *nextaddr;

        for (curdev = alldevs; curdev != NULL; curdev = nextdev) {
                nextdev = curdev->next;

                /*
                 * Free all addresses.
                 */
                for (curaddr = curdev->addresses; curaddr != NULL; curaddr = nextaddr) {
                        nextaddr = curaddr->next;
                        if (curaddr->addr)
                                free(curaddr->addr);
                        if (curaddr->netmask)
                                free(curaddr->netmask);
                        if (curaddr->broadaddr)
                                free(curaddr->broadaddr);
                        if (curaddr->dstaddr)
                                free(curaddr->dstaddr);
                        free(curaddr);
                }

                /*
                 * Free the name string.
                 */
                free(curdev->name);

                /*
                 * Free the description string, if any.
                 */
                if (curdev->description != NULL)
                        free(curdev->description);

                /*
                 * Free the interface.
                 */
                free(curdev);
        }
}

//XXX - returns linktype which is always LINKTYPE_ETHERNET
int pcap_datalink(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

        if (!p->activated)
                return (PCAP_ERROR_NOT_ACTIVATED);
        return (p->linktype);
}

const char *
pcap_datalink_val_to_name(int dlt)
{
	debug("[%s() start]\n", __func__);

        int i;

        for (i = 0; dlt_choices[i].name != NULL; i++) {
                if (dlt_choices[i].dlt == dlt)
                        return (dlt_choices[i].name);
        }
        return (NULL);
}

const char *
pcap_datalink_val_to_description(int dlt)
{
	debug("[%s() start]\n", __func__);

        int i;

        for (i = 0; dlt_choices[i].name != NULL; i++) {
                if (dlt_choices[i].dlt == dlt)
                        return (dlt_choices[i].description);
        }
        return (NULL);
}

void pcap_close(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

	if (p->packet)
		trace_destroy_packet(p->packet);
	if (p->trace)
		trace_destroy(p->trace);
        free(p);
}

//@source - iface name.
pcap_t *pcap_create(const char *source, char *errbuf)
{
	debug("[%s() start]\n", __func__);

	pcap_t *handle;
	handle = malloc(sizeof(pcap_t));
	if (!handle)
		return NULL;

	//init our pcap structure
	strcpy(handle->name, source);
	handle->activated = 0;
	handle->packet = NULL;
	handle->linktype = LINKTYPE_ETHERNET;
	handle->snapshot = 65536;
	handle->fd = 7777;	//XXX
	handle->trace_out = NULL;

        /* Creating and initialising a packet structure to store the packets
         * that we're going to read from the trace. We store all packets here
	 * alloc memory for packet and clear its fields */
	handle->packet = trace_create_packet();
	if (!handle->packet)
	{
		printf("failed to create packet (storage)\n");
		return NULL;
	}

	handle->trace = trace_create(source);
	if (!handle->trace)
	{
		printf("failed to create trace\n");
		return NULL;
	}
	else
		printf("trace created successfully\n");

	return handle;
}

// just call the pcap_create() as in libtrace trace_create() 
// opens both: network interface and offline file for reading.
pcap_t *pcap_open_offline(const char *fname, char *errbuf)
{
	debug("[%s() start]\n", __func__);

	return pcap_create(fname, errbuf);
}

//internal function, not in my list
int pcap_check_activated(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

        if (p->activated) 
	{
                printf("can't perform operation on activated capture\n");
                return (-1);
        }
        return (0);
}

int pcap_set_snaplen(pcap_t *p, int snaplen)
{
	debug("[%s() start]\n", __func__);

        if (pcap_check_activated(p))
                return (PCAP_ERROR_ACTIVATED);

        /*
         * Turn invalid values, or excessively large values, into
         * the maximum allowed value.
         *
         * If some application really *needs* a bigger snapshot
         * length, we should just increase MAXIMUM_SNAPLEN.
         */
        if (snaplen <= 0 || snaplen > MAXIMUM_SNAPLEN)
                snaplen = MAXIMUM_SNAPLEN;
        p->snapshot = snaplen;
        return (0);
}

//we just have a define in libtrace, but not a function to set buf size,
//so this is just stub here
//#define LIBTRACE_PACKET_BUFSIZE   65536

int pcap_set_buffer_size(pcap_t *p, int buffer_size)
{
	debug("[%s() start]\n", __func__);

        if (pcap_check_activated(p))
                return (PCAP_ERROR_ACTIVATED);
        if (buffer_size <= 0) {
                /*
                 * Silently ignore invalid values.
                 */
                return (0);
        }
	//XXX - don't do actually nothing here
        //p->opt.buffer_size = buffer_size;
        return (0);
}

int pcap_set_promisc(pcap_t *p, int promisc)
{
	debug("[%s() start]\n", __func__);

        if (pcap_check_activated(p))
                return (PCAP_ERROR_ACTIVATED);

	trace_set_promisc(p->trace, (bool)promisc);

        return (0);
}

//so this is just stub here
int pcap_set_timeout(pcap_t *p, int timeout_ms)
{
	debug("[%s() start]\n", __func__);

        if (pcap_check_activated(p))
                return (PCAP_ERROR_ACTIVATED);
	//XXX - don't do actually nothing here
        //p->opt.timeout = timeout_ms;
        return (0);
}

int pcap_activate(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

	int rv;

	p->activated = 1;
	rv = trace_start(p->trace);
	fprintf(stderr, "%s() rv: %d \n", __func__, rv);

	return rv;
}

int pcap_snapshot(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

        if (!p->activated)
                return (PCAP_ERROR_NOT_ACTIVATED);
        return (p->snapshot);
}

int pcap_fileno(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

        return (p->fd);
}

int pcap_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
	debug("[%s() start]\n", __func__);

        int ret;

	ret = nonblock;
//stub  
#if 0
        ret = p->setnonblock_op(p, nonblock, errbuf);
        if (ret == -1) { 
                /*
                 * In case somebody depended on the bug wherein
                 * the error message was put into p->errbuf
                 * by pcap_setnonblock_fd().
                 */
                strlcpy(p->errbuf, errbuf, PCAP_ERRBUF_SIZE);
        }           
#endif
        return (ret);
}



#if 0
struct pcap_pkthdr {
        struct timeval ts;      /* time stamp */
        bpf_u_int32 caplen;     /* length of portion present */
        bpf_u_int32 len;        /* length this packet (off wire) */
};
#endif

int pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	debug("[%s() start]\n", __func__);

	int i = 0;
	int rv = 0;
	int pkts = 0;

	while (i < cnt)
	{
		//we return number of bytes read
		rv = trace_read_packet(p->trace, p->packet);
		//XXX - call callback for every packet here
		if (rv > 0)
			pkts++;
		else
			pkts = rv;
	}

	return pkts;
}

//pcap_next() reads the next packet (by calling pcap_dispatch() with a cnt of 1) and returns a u_char pointer to the data in that packet.
//The bytes of data from the packet begin with a link-layer header
const u_char *pcap_next(pcap_t *p, struct pcap_pkthdr *h)
{
	debug("[%s() start]\n", __func__);

	int rv;

	//trace_read_packet (libtrace_t *trace, libtrace_packet_t *packet)
	//will block until a packet is read (or EOF is reached).
	rv = trace_read_packet(p->trace, p->packet);
	if (rv == 0)
		printf("EOF, no packets\n");
	else if (rv < 0)
		printf("error reading packet\n");
	else
	{
		h->len = trace_get_capture_length(p->packet);
	}

	if (rv)
		return (u_char*)p->packet;
	else
		return NULL;
}

char *pcap_geterr(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

	static libtrace_err_t err;

	err = trace_get_err(p->trace);
	
	return err.problem;
}

//XXX - stub
int pcap_inject(pcap_t *p, const void *buf, size_t size)
{
	debug("[%s() start]\n", __func__);

	int rv = -1;
	
	//XXX - no such func in libtrace


	return rv;
}

//Force the loop in "pcap_read()" or "pcap_read_offline()" to terminate.
void pcap_breakloop(pcap_t *p)
{
	debug("[%s() start]\n", __func__);

        //p->break_loop = 1;

	trace_interrupt();
}

int
pcap_dump_flush(pcap_dumper_t *p)
{
	debug("[%s() start]\n", __func__);


        if (fflush((FILE *)p) == EOF)
                return (-1);
        else
                return (0);
}

void pcap_dump_close(pcap_dumper_t *p)
{
	debug("[%s() start]\n", __func__);

        (void)fclose((FILE *)p);
}

//pcap_dump_open, pcap_dump_fopen - open a file to which to write packets
//pcap:/path/to/pcap/file
pcap_dumper_t * pcap_dump_open(pcap_t *p, const char *fname)
{
	debug("[%s() start]\n", __func__);

	char fulluri[256] = "pcap:";
	strcat(fulluri, fname);
	p->trace_out = trace_create_output(fulluri);

	//XXX This generally creates the output file. So if we want to create
	//valid output - we also have to call:
	//trace_start_output()
}


int pcap_lookupnet(device, netp, maskp, errbuf)
        register const char *device;
        register bpf_u_int32 *netp, *maskp;
        register char *errbuf;
{
	debug("[%s() start]\n", __func__);

        register int fd;
        register struct sockaddr_in *sin4;
        struct ifreq ifr;

        /*
         * The pseudo-device "any" listens on all interfaces and therefore
         * has the network address and -mask "0.0.0.0" therefore catching
         * all traffic. Using NULL for the interface is the same as "any".
         */
        if (!device || strcmp(device, "any") == 0)
	{
                *netp = *maskp = 0;
                return 0;
        }

        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
                (void)snprintf(errbuf, PCAP_ERRBUF_SIZE, "socket fckd up");
                return (-1);
        }
        memset(&ifr, 0, sizeof(ifr));

        ifr.ifr_addr.sa_family = AF_INET;

        (void)strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
        if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
                if (errno == EADDRNOTAVAIL) {
                        (void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
                            "%s: no IPv4 address assigned", device);
                } else {
                        (void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
                            "SIOCGIFADDR: %s: %s",
                            device, pcap_strerror(errno));
                }
                (void)close(fd);
                return (-1);
        }
        sin4 = (struct sockaddr_in *)&ifr.ifr_addr;
        *netp = sin4->sin_addr.s_addr;
        memset(&ifr, 0, sizeof(ifr));

        /* XXX Work around Linux kernel bug */
        ifr.ifr_addr.sa_family = AF_INET;

        (void)strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
        if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifr) < 0) {
                (void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
                    "SIOCGIFNETMASK: %s: %s", device, pcap_strerror(errno));
                (void)close(fd);
                return (-1);
        }
        (void)close(fd);
        *maskp = sin4->sin_addr.s_addr;
        if (*maskp == 0) {
                if (IN_CLASSA(*netp))
                        *maskp = IN_CLASSA_NET;
                else if (IN_CLASSB(*netp))
                        *maskp = IN_CLASSB_NET;
                else if (IN_CLASSC(*netp))
                        *maskp = IN_CLASSC_NET;
                else {
                        (void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
                            "inet class for 0x%x unknown", *netp);
                        return (-1);
                }
        }
        *netp &= *maskp;

        return (0);
}

//we call 1. pcap_compile() to convert our string into bfp.
//then we call 2. pcap_setfilter() to set bpf filter

#if 0
/** Internal representation of a BPF filter */
struct libtrace_filter_t {
        struct bpf_program filter;      /**< The BPF program itself */
        char * filterstring;            /**< The filter string */
        int flag;                       /**< Indicates if the filter is valid */
        struct bpf_jit_t *jitfilter;
};
#endif

//@fp - OUT, @str - input string to be converted into filter
int pcap_compile(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, bpf_u_int32 netmask)
{
	debug("[%s() start]\n", __func__);

	int rv;

	//libtrace_filter_t * 	trace_create_filter (const char *filterstring)
	libtrace_filter_t *filter = trace_create_filter(str);

	//XXX - we really return pointer to another type, but in libtrace_filter_t struct
	//we have bpf_program on first place, so in theory it should work fine
	fp = (struct bpf_program*)filter;

	if (filter)
		rv = 0;
	else
		rv = -1;

	return rv;		

}

int pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{
	debug("[%s() start]\n", __func__);

	int rv;

	libtrace_filter_t *filter = (struct libtrace_filter_t*)fp;

	//int 	trace_apply_filter (libtrace_filter_t *filter, const libtrace_packet_t *packet)
	rv = trace_apply_filter(filter, p->packet);
	if (rv > 0)
		return 0;
	else
		return -1;
}

void pcap_freecode(struct bpf_program *program)
{
	debug("[%s() start]\n", __func__);

	//void 	trace_destroy_filter (libtrace_filter_t *filter)

	libtrace_filter_t *filter = (struct libtrace_filter_t*)program;

	trace_destroy_filter(filter);
}

int pcap_stats(pcap_t *p, struct pcap_stat *ps)
{
	debug("[%s() start]\n", __func__);

	int rv = 0;

	libtrace_stat_t *stat;

	stat = trace_get_statistics(p->trace, NULL);
	if (stat)
	{
		ps->ps_recv = (unsigned int)(stat->received);
		ps->ps_drop = (unsigned int)(stat->dropped);
		ps->ps_ifdrop = (unsigned int)(stat->filtered); //filtered out
	}
	else
		rv = -1;

	return rv;
}
