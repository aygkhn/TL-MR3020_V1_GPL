/******************************************************************************
 *
 * Copyright (c) 2008-2008 TP-Link Technologies CO.,LTD.
 * All rights reserved.
 *
 * �ļ�����:	ipt_multiurl.c
 * ��    ��:	1.0
 * ժ    Ҫ:	netfilter kernel module
 *
 * ��    ��:	ZJin <zhongjin@tp-link.net>
 * ��������:	10/23/2008
 *
 * �޸���ʷ:
----------------------------
 *
 ******************************************************************************/

/* for example */
/*
	iptables -A FORWARD -m multiurl --urls www.baidu.com,sina,163 -p tcp --dport 80 -j ACCEPT

	netfilter will find the urls in every http pkt, if found, means matched!
*/

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_multiurl.h>

#include <linux/ip.h>
#include <linux/tcp.h>

#define	HOST_STR	"Host: "

/*
start: the search area start mem addr;
end:	the search area end mem addr;

strCharSet:
Null-terminated string to search for
*/
u_int8_t *url_strstr(const u_int8_t* start, const u_int8_t* end, const u_int8_t* strCharSet)
{
	/* [liaoxingwei start] change BF to kmp */
	int pos;
	struct ts_config *conf;
	struct ts_state state;
	const char *pattern = "chicken";
	const char *example = "We dance the funky chicken";

	conf = textsearch_prepare("kmp", strCharSet, strlen(strCharSet),
                             GFP_KERNEL, TS_AUTOLOAD);
	if (IS_ERR(conf))
	{
		textsearch_destroy(conf);
		return NULL;
	}

	pos = textsearch_find_continuous(conf, &state, start, end - start + 1);
	if (pos != UINT_MAX)
	{
		textsearch_destroy(conf);
		return start + pos;
	}

	textsearch_destroy(conf);
	return NULL;
	/* [liaoxingwei end] */
}


static bool match(const struct sk_buff *skb, 
      const struct xt_match_param *param)
{	
	const struct xt_multiurl_info *info = param->matchinfo;		

	const struct iphdr *iph = ip_hdr(skb);

    struct tcphdr *tcph = (void *)iph + iph->ihl*4;	/* Might be TCP, UDP */

	u_int8_t*	http_payload_start = NULL;
	u_int8_t*	http_payload_end = NULL;

	
	u_int8_t*	host_str_start = NULL;
	u_int8_t*	url_start = NULL;
	u_int8_t*	url_end = NULL;
	u_int8_t*	crlf_pos = NULL;
	
	//int found_crlf = 0;
	
	int i;
	
	/* return ack/syn/fin pkts ZJin 090409 */
	if (iph->tot_len == iph->ihl*4 + tcph->doff*4)
	{
		//printk("1st-if: ack/syn/fin pkts!\n");
		/* return 1 or 0 based the last "FLAG" string. ZJin090417 */
		if (!memcmp(info->urls[info->url_count - 1], "return1", 7))
		return 1;
		else
			return 0;
	}
	
	http_payload_start = (u_int8_t *)tcph + tcph->doff*4;
	http_payload_end = http_payload_start + (iph->tot_len - iph->ihl*4 - tcph->doff*4) - 1;

#if 0
	/* usually it's a ack/syn pkt */
	if (http_payload_start == http_payload_end)
	{
		//printk("1st-if: not a get pkt!\n");
		return 1;
	}
#endif

	/* find \r\nHost: in pkt */
	host_str_start = url_strstr(http_payload_start, http_payload_end, HOST_STR);

	if (host_str_start != NULL)
	{
		url_start = host_str_start + 6;

		crlf_pos = url_strstr(url_start, http_payload_end, "\r\n");

		if (crlf_pos == NULL)
			return 0;	/* Host without \r\n... */

		/* the end of "www.xxx.com" */
		url_end = crlf_pos - 1;
		#if 0
		crlf_pos = url_start;

		while (0 == found_crlf)
		{
			if ((*crlf_pos == '\r') && (*(crlf_pos + 1) == '\n'))
			{
				found_crlf = 1;
			}
			crlf_pos++;
		}
		/* the end of "www.xxx.com" */
		url_end = crlf_pos - 2;
		#endif
		/* ZJin: 090417 the last "URL" just is a FLAG */
		for (i = 0; i < info->url_count - 1; ++i)
		{
			if (url_strstr(url_start, url_end, info->urls[i]))
			{
				//printk("==== matched %s ====\n", info->urls[i]);
				
				return 1;
			}
		}
	}
	else
	{
		return 0;	/* ��get����û��"Host: "��ֱ����Ϊ��ƥ�䡣 */
	}
	
	return 0;

}


/* Called when user tries to insert an entry of this type. */

static bool checkentry(const struct xt_mtchk_param *param)
{
	if (param->match->matchsize != XT_ALIGN(sizeof(struct xt_multiurl_info)))		
		return 0;

	return 1;
}


static struct xt_match multiurl_match = { 
    .name           = "multiurl",
    .family			= NFPROTO_IPV4,
    .match          = &match,
    .checkentry     = &checkentry,
    .matchsize		= XT_ALIGN(sizeof(struct xt_multiurl_info)),
    .me             = THIS_MODULE,
};

static int __init init(void)
{
	return xt_register_match(&multiurl_match);
}

static void __exit fini(void)
{
	xt_unregister_match(&multiurl_match);
}

module_init(init);
module_exit(fini);

MODULE_AUTHOR("ZJin");
MODULE_DESCRIPTION("A module to match MultiURL in host section of http get!.");
MODULE_LICENSE("GPL");
