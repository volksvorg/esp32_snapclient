/*
 * sntp_handler.h
 *
 *  Created on: 12.08.2022
 *      Author: florian
 */

#ifndef myMDNS_H_
#define myMDNS_H_

void initialise_mdns(void);
void query_mdns_service(const char * service_name, const char * proto);

#endif /* MNDS_H_ */
