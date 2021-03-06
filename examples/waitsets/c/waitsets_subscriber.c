/* waitsets_subscriber.c

   A subscription example

   This file is derived from code automatically generated by the rtiddsgen 
   command:

   rtiddsgen -language C -example <arch> waitsets.idl

   Example subscription of type waitsets automatically generated by 
   'rtiddsgen'. To test them, follow these steps:

   (1) Compile this file and the example publication.

   (2) Start the subscription with the command
       objs/<arch>/waitsets_subscriber <domain_id> <sample_count>

   (3) Start the publication with the command
       objs/<arch>/waitsets_publisher <domain_id> <sample_count>

   (4) [Optional] Specify the list of discovery initial peers and 
       multicast receive addresses via an environment variable or a file 
       (in the current working directory) called NDDS_DISCOVERY_PEERS. 
       
   You can run any number of publishers and subscribers programs, and can 
   add and remove them dynamically from the domain.
              
                                   
   Example:
        
       To run the example application on domain <domain_id>:
                          
       On UNIX systems: 
       
       objs/<arch>/waitsets_publisher <domain_id> 
       objs/<arch>/waitsets_subscriber <domain_id> 
                            
       On Windows systems:
       
       objs\<arch>\waitsets_publisher <domain_id>  
       objs\<arch>\waitsets_subscriber <domain_id>   
       
       
modification history
------------ -------       
*/

#include <stdio.h>
#include <stdlib.h>
#include "ndds/ndds_c.h"
#include "waitsets.h"
#include "waitsetsSupport.h"

/* We don't need to use listeners as we are going to use Waitsets and Conditions
 * so we have removed the auto generated code for listeners here */

/* Delete all entities */
static int subscriber_shutdown(
    DDS_DomainParticipant *participant)
{
    DDS_ReturnCode_t retcode;
    int status = 0;

    if (participant != NULL) {
        retcode = DDS_DomainParticipant_delete_contained_entities(participant);
        if (retcode != DDS_RETCODE_OK) {
            printf("delete_contained_entities error %d\n", retcode);
            status = -1;
        }

        retcode = DDS_DomainParticipantFactory_delete_participant(
            DDS_TheParticipantFactory, participant);
        if (retcode != DDS_RETCODE_OK) {
            printf("delete_participant error %d\n", retcode);
            status = -1;
        }
    }

    /* RTI Connext provides the finalize_instance() method on
       domain participant factory for users who want to release memory used
       by the participant factory. Uncomment the following block of code for
       clean destruction of the singleton. */
/*
    retcode = DDS_DomainParticipantFactory_finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        printf("finalize_instance error %d\n", retcode);
        status = -1;
    }
*/

    return status;
}

static int subscriber_main(int domainId, int sample_count)
{
    DDS_DomainParticipant *participant = NULL;
    DDS_Subscriber *subscriber = NULL;
    DDS_Topic *topic = NULL;
    DDS_DataReader *reader = NULL;
    DDS_ReturnCode_t retcode;
    const char *type_name = NULL;
    int count = 0;
    DDS_ReadCondition *read_condition;
    DDS_StatusCondition *status_condition;
    DDS_WaitSet *waitset = NULL;
    waitsetsDataReader *waitsets_reader = NULL;
    struct DDS_Duration_t wait_timeout = {1,500000000};
    /* To change the DataReader's QoS programmatically you will need to
     * declare and initialize datareader_qos here. */
    struct DDS_DataReaderQos datareader_qos = DDS_DataReaderQos_INITIALIZER;


    /* To customize participant QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    participant = DDS_DomainParticipantFactory_create_participant(
        DDS_TheParticipantFactory, domainId, &DDS_PARTICIPANT_QOS_DEFAULT,
        NULL /* listener */, DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        printf("create_participant error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* To customize subscriber QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    subscriber = DDS_DomainParticipant_create_subscriber(
        participant, &DDS_SUBSCRIBER_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (subscriber == NULL) {
        printf("create_subscriber error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Register the type before creating the topic */
    type_name = waitsetsTypeSupport_get_type_name();
    retcode = waitsetsTypeSupport_register_type(participant, type_name);
    if (retcode != DDS_RETCODE_OK) {
        printf("register_type error %d\n", retcode);
        subscriber_shutdown(participant);
        return -1;
    }

    /* To customize topic QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    topic = DDS_DomainParticipant_create_topic(
        participant, "Example waitsets",
        type_name, &DDS_TOPIC_QOS_DEFAULT, NULL /* listener */,
        DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        printf("create_topic error\n");
        subscriber_shutdown(participant);
        return -1;
    }


    /* To customize data reader QoS, use 
       the configuration file USER_QOS_PROFILES.xml */
    reader = DDS_Subscriber_create_datareader(
        subscriber, DDS_Topic_as_topicdescription(topic),
        &DDS_DATAREADER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (reader == NULL) {
        printf("create_datareader error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* If you want to change the DataReader's QoS programmatically rather than
     * using the XML file, you will need to add the following lines to your
     * code and comment out the create_datareader call above.
     *
     * In this case, we reduce the liveliness timeout period to trigger the
     * StatusCondition DDS_LIVELINESS_CHANGED_STATUS
     */

    /*
    retcode = DDS_Subscriber_get_default_datareader_qos(subscriber, &datareader_qos);
    if (retcode != DDS_RETCODE_OK) {
        printf("get_default_datareader_qos error\n");
        return -1;
    }

    datareader_qos.liveliness.lease_duration.sec = 2;
    datareader_qos.liveliness.lease_duration.nanosec = 0;

    reader = DDS_Subscriber_create_datareader(
        subscriber, DDS_Topic_as_topicdescription(topic),
        &datareader_qos, NULL, DDS_STATUS_MASK_NONE);
    if (reader == NULL) {
        printf("create_datareader error\n");
        subscriber_shutdown(participant);
        return -1;
    }
    */

    /* Create read condition
     * ---------------------
     * Note that the Read Conditions are dependent on both incoming
     * data as well as sample state. Thus, this method has more
     * overhead than adding a DDS_DATA_AVAILABLE_STATUS StatusCondition.
     * We show it here purely for reference
     */
    read_condition = DDS_DataReader_create_readcondition(
        reader,
        DDS_NOT_READ_SAMPLE_STATE,
        DDS_ANY_VIEW_STATE,
        DDS_ANY_INSTANCE_STATE);
    if (read_condition == NULL) {
        printf("create_readcondition error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Get status conditions
     * ---------------------
     * Each entity may have an attached Status Condition. To modify the
     * statuses we need to get the reader's Status Conditions first.
     */
    status_condition = DDS_Entity_get_statuscondition((DDS_Entity*) reader);
    if (status_condition == NULL) {
        printf("get_statuscondition error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Set enabled statuses
     * --------------------
     * Now that we have the Status Condition, we are going to enable the
     * statuses we are interested in: DDS_SUBSCRIPTION_MATCHED_STATUS and
     * DDS_LIVELINESS_CHANGED_STATUS.
     */
    retcode = DDS_StatusCondition_set_enabled_statuses(
        status_condition,
        DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);
    if (retcode != DDS_RETCODE_OK) {
        printf("set_enabled_statuses error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Create and attach conditions to the WaitSet
     * -------------------------------------------
     * Finally, we create the WaitSet and attach both the Read Conditions
     * and the Status Condition to it.
     */
    waitset = DDS_WaitSet_new();
    if (waitset == NULL) {
        printf("create waitset error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Attach Read Conditions */
    retcode = DDS_WaitSet_attach_condition(waitset,
            (DDS_Condition*)read_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("attach_condition error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Attach Status Conditions */
    retcode = DDS_WaitSet_attach_condition(waitset,
            (DDS_Condition*)status_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("attach_condition error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Narrow the reader into your specific data type */
    waitsets_reader = waitsetsDataReader_narrow(reader);
    if (waitsets_reader == NULL) {
        printf("DataReader narrow error\n");
        return -1;
    }


    /* Main loop */
    for (count=0; (sample_count == 0) || (count < sample_count); ++count) {
        struct DDS_ConditionSeq active_conditions_seq =
                DDS_SEQUENCE_INITIALIZER;
        int i, j, active_conditions;

        /* wait() blocks execution of the thread until one or more attached
         * Conditions become true, or until a user-specified timeout expires.
         */
         retcode = DDS_WaitSet_wait(
                 waitset,                       /* waitset */
                 &active_conditions_seq,        /* active conditions sequence */
                 &wait_timeout);                /* timeout */
         /* We get to timeout if no conditions were triggered */
         if (retcode == DDS_RETCODE_TIMEOUT) {
             printf("Wait timed out!! No conditions were triggered.\n");
             continue;
         } else if (retcode != DDS_RETCODE_OK) {
             printf("wait returned error: %d", retcode);
             break;
         }

         /* Get the number of active conditions */
         active_conditions =
                 DDS_ConditionSeq_get_length(&active_conditions_seq);

         printf("Got %d active conditions\n", active_conditions);

         for (i = 0; i<active_conditions; ++i) {
             /* Get the current condition and compare it with the Status
              * Conditions and the Read Conditions previously defined. If
              * they match, we print the condition that was triggered.*/
             DDS_Condition * current_condition =
                     DDS_ConditionSeq_get(&active_conditions_seq, i);

             /* Compare with Status Conditions */
             if (current_condition == (DDS_Condition *) status_condition) {
                 DDS_StatusMask triggeredmask;
                 /* Get the status changes so we can check which status
                  * condition triggered. */
                 triggeredmask =
                         DDS_Entity_get_status_changes(
                                 (DDS_Entity*)waitsets_reader);

                 /* Liveliness changed */
                 if (triggeredmask & DDS_LIVELINESS_CHANGED_STATUS) {
                     struct DDS_LivelinessChangedStatus st;
                     DDS_DataReader_get_liveliness_changed_status(
                         (DDS_DataReader*)waitsets_reader, &st);
                     printf("Liveliness changed => Active writers = %d\n",
                            st.alive_count);
                 }

                 /* Subscription matched */
                 if (triggeredmask & DDS_SUBSCRIPTION_MATCHED_STATUS) {
                     struct DDS_SubscriptionMatchedStatus st;
                     DDS_DataReader_get_subscription_matched_status(
                         (DDS_DataReader*)waitsets_reader, &st);
                     printf("Subscription matched => Cumulative matches = %d\n",
                            st.total_count);

                 }
             }

             /* Compare with Read Conditions */
             else if (current_condition == (DDS_Condition *) read_condition) {
                 printf("Read condition\n");
                 /* Current conditions match our conditions to read data, so
                  * we can read data just like we would do in any other
                  * example. */
                 struct waitsetsSeq data_seq = DDS_SEQUENCE_INITIALIZER;
                 struct DDS_SampleInfoSeq info_seq = DDS_SEQUENCE_INITIALIZER;

                 /* You may want to call take_w_condition() or
                  * read_w_condition() on the Data Reader. This way you will use
                  * the same status masks that were set on the Read Condition.
                  * This is just a suggestion, you can always use
                  * read() or take() like in any other example.
                  */
                 retcode = waitsetsDataReader_take_w_condition(
                     waitsets_reader, &data_seq, &info_seq,
                     DDS_LENGTH_UNLIMITED, read_condition);
                 if (retcode != DDS_RETCODE_OK) {
                     printf("take error %d\n", retcode);
                     break;
                 }

                 for (j = 0; j < waitsetsSeq_get_length(&data_seq); ++j) {
                     if (!DDS_SampleInfoSeq_get_reference(&info_seq, j)->valid_data) {
                         printf("Got metadata\n");
                         continue;
                     }
                     waitsetsTypeSupport_print_data(
                         waitsetsSeq_get_reference(&data_seq, j));
                 }

                 retcode = waitsetsDataReader_return_loan(
                     waitsets_reader, &data_seq, &info_seq);
                 if (retcode != DDS_RETCODE_OK) {
                     printf("return loan error %d\n", retcode);
                 }
             }
         }

    }

    /* Delete all entities */
    retcode = DDS_WaitSet_delete(waitset);
    if (retcode != DDS_RETCODE_OK) {
        printf("delete waitset error %d\n", retcode);
    }

    /* Cleanup and delete all entities */ 
    return subscriber_shutdown(participant);
}

#if defined(RTI_WINCE)
int wmain(int argc, wchar_t** argv)
{
    int domainId = 0;
    int sample_count = 0; /* infinite loop */
    
    if (argc >= 2) {
        domainId = _wtoi(argv[1]);
    }
    if (argc >= 3) {
        sample_count = _wtoi(argv[2]);
    }

    /* Uncomment this to turn on additional logging
    NDDS_Config_Logger_set_verbosity_by_category(
        NDDS_Config_Logger_get_instance(),
        NDDS_CONFIG_LOG_CATEGORY_API, 
        NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
    */
    
    return subscriber_main(domainId, sample_count);
}
#elif !(defined(RTI_VXWORKS) && !defined(__RTP__)) && !defined(RTI_PSOS)
int main(int argc, char *argv[])
{
    int domainId = 0;
    int sample_count = 0; /* infinite loop */

    if (argc >= 2) {
        domainId = atoi(argv[1]);
    }
    if (argc >= 3) {
        sample_count = atoi(argv[2]);
    }

    /* Uncomment this to turn on additional logging
    NDDS_Config_Logger_set_verbosity_by_category(
        NDDS_Config_Logger_get_instance(),
        NDDS_CONFIG_LOG_CATEGORY_API, 
        NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
    */
    
    return subscriber_main(domainId, sample_count);
}
#endif

#ifdef RTI_VX653
const unsigned char* __ctype = NULL;

void usrAppInit ()
{
#ifdef  USER_APPL_INIT
    USER_APPL_INIT;         /* for backwards compatibility */
#endif
    
    /* add application specific code here */
    taskSpawn("sub", RTI_OSAPI_THREAD_PRIORITY_NORMAL, 0x8, 0x150000, (FUNCPTR)subscriber_main, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
   
}
#endif
