#!/usr/bin/env python
# coding: utf-8

import gym
import argparse
import numpy as np
np.random.seed(1)
from ns3gym import ns3env
from DQN_model import DeepQNetwork
from DQN_model import Eval_Model
from DQN_model import Target_Model
import tensorflow as tf

import matplotlib.pyplot as plt
import os
import time
# os.environ["CUDA_VISIBLE_DEVICES"] = str(1) # use GPU 1
# config = tf.compat.v1.ConfigProto()
# config.gpu_options.allow_growth=True
# #config.gpu_options.per_process_gpu_memory_fraction = 0.8
# sess = tf.compat.v1.Session(config=config)
# tf.compat.v1.keras.backend.set_session(sess)
print(tf.config.list_physical_devices('GPU'))
os.environ["CUDA_VISIBLE_DEVICES"] = str("1") # use GPU 1
gpu_devices = tf.config.experimental.list_physical_devices('GPU')
for device in gpu_devices:
    tf.config.experimental.set_memory_growth(device, True)


# loss function learning rate
learning_rate = 0.001
# discount factor
reward_decay = 0.9
# 10% random select
e_greedy = 0.9
# copy weight from eval_net to target_net each N iter
replace_target_iter = 100
# size of experience pool
memory_size = 5000
batch_size = 32
training_episodes = 20
# MAXSLOTS
MAXSLOTS = 3

episode_time = 600
# model path (Need to change)
eval_model_weights_path = 'model_weight/15pkt/eval_model_weights'
target_model_weights_path = 'model_weight/15pkt/target_model_weights'
collect_data = True
if collect_data:
    training_episodes = 1
# select slot only base on distance
def slot_scoring(obs):
    #values = np.random.random_sample((32,)) / 20
    
    values = np.zeros((32,)) 
    #values[np.nonzero(obs[:n_slotUsedTable])] = -np.inf
    
    top_index = []
    for i in range(3):
        top_index.append(np.ravel(np.argwhere(obs[:n_slotUsedTable]==i+1)))
    
    for act in range(32):
        r = 0
        max_r = 0
        for i in range(3):
            if any(act < idx for idx in top_index[i]):
                distance_map = top_index[i] - act
                distance = min(np.where(distance_map>0,distance_map,np.inf))
                closest_distance = 0
                for idx in range(closest_distance):
                    closest_distance = closest_distance + 1 if obs[act+idx] == 0 else closest_distance
                    
                distance_reward = (1.5-pow((closest_distance/10),0.2))
                size_weight = _obs[-(3-i)]
                max_r =  max((distance_reward*size_weight/3 + throughput/max_throughput/2),max_r) 

                        

        values[act] = r + max_r
    
    values[np.nonzero(obs[:n_slotUsedTable])] = -np.inf
    
    #print ("obs:",obs[:n_slotUsedTable])
    #print ("values:",values)
    
    return values

# random select N slot 
def random_select(obs, max_slot):
    nonusedSlot = np.where(obs==0)[0]
    action = np.sort(np.random.choice(nonusedSlot,max_slot,replace=False))
    
    return action

# slot selection module
def slot_select(q_values, free_slotNum, slot_table):
    # get top N value of q value
    q_values[np.nonzero(slot_table)] = -np.inf
    action_unsort = np.argpartition(q_values,-MAXSLOTS)[-MAXSLOTS:]
    
    # sort action num by q value
    action = action_unsort[np.argsort(-1*q_values[action_unsort])]
    
    
    max_choosable_slotNum = min(free_slotNum, MAXSLOTS)
    # Using the queuing bytes to determine how many data slot would be choose
    action_num = 0 if queueBytes == 0 else min(max(0, int(queueBytes/6250 - 0.3)) + 1, max_choosable_slotNum)
        
    action[action_num:] = [-1] * (MAXSLOTS-action_num)
    
    # Sort action num
    action[:action_num] = np.sort(action[:action_num])
    
    return action

throughput_by_pktNum = []
delay_by_pktNum = []
enqueue_drop_by_pktNum = []
cleanup_drop_by_pktNum = []
for i in range (6): # {0,1,2,3,4,5} -> {5,10,15,20,25,30}
    optimized_choice = []
    throughput_list = []
    throughput_history = []
    delay_list = []
    delay_history = []
    enqueue_drop_list = []
    enqueue_drop_history = []
    cleanup_drop_list = []
    cleanup_drop_history = []
    
    pktNum = (i+1)*5
    max_throughput = 25*576*pktNum #  send_25times_per_sec*packetsize*packetnum
    
    simArgs = {'pktNum': pktNum}
    #env = gym.make('ns3-v0') # issue of socket
    env = ns3env.Ns3Env(simArgs=simArgs,debug=True)

    #env.reset()
    ob_space = env.observation_space
    ac_space = env.action_space
    ob_space_n = ob_space['slotUsedTable'].shape[0] + ob_space['pktBytes'].shape[0] - 1
    n_slotUsedTable = ob_space['slotUsedTable'].shape[0]


    print("Observation space: ", ob_space,  ob_space.dtype)
    print("Action space: ", ac_space, ac_space.dtype)
    print("n_action: ",ac_space.shape[0])
    
    # initiailize eval_net target_net
    eval_model = Eval_Model(num_actions=ac_space.shape[0])
    target_model = Target_Model(num_actions=ac_space.shape[0])
    try:
        # load weight from file
        eval_model.load_weights(eval_model_weights_path)
        target_model.load_weights(target_model_weights_path)
        print('Load weights from ',eval_model_weights_path," and ",target_model_weights_path)
    except:
        print('Create new model')

    # create DQN model
    RL = DeepQNetwork(ac_space.shape[0], MAXSLOTS, ob_space_n,
                      eval_model, target_model, learning_rate, reward_decay, e_greedy, 
                      replace_target_iter, memory_size, batch_size)

    for episode in range(training_episodes):
        # For each 10 round, would have 1 round stop update model and select slot without 10% random select
        if collect_data:
            isTraining = False
        else :
            isTraining = True if (episode+1)%10 != 0 else False

        if isTraining:
            print ("-----------------------episodes: ", episode, " (Training)----------------------")
        else:
            print ("-----------------------episodes: ", episode, " (Testing)-----------------------")

        stepIdx = 0
        total_choice_counter = 0
        optimized_choice_counter = 0
        nonLearn_data = 0
        throughput = 0
        delay = 0
        enqueue_drop = 0
        cleanup_drop = 0
        
        # Initial environment
        _obs = env.reset()
        queueBytes = _obs[1][0]

        free_slotNum = min(n_slotUsedTable - np.nonzero(_obs[0])[0].size,MAXSLOTS)

        _obs = np.array(list(_obs[0]) + list(_obs[1][1:]))
        _obs = np.pad(_obs,(0, ob_space_n - _obs.size), constant_values = 0)


        while True:
            stepIdx += 1

            # Get Action
            # Select slot by RL algorithm
            action = np.array(slot_select(RL.choose_action(_obs, isTraining),free_slotNum,_obs[:n_slotUsedTable]))

            # Select slot by random select 2 slot
            #action = np.array(random_select(_obs,2))
            # Select slot by random select with queued information
            #action = np.array(slot_select(np.random.random_sample(ac_space.shape[0]),free_slotNum,_obs[:n_slotUsedTable]))
            # Select slot by rule (distance reward)
            #action = np.array(slot_select(slot_scoring(_obs),free_slotNum,_obs[:n_slotUsedTable]))

            #print("---action: ", action)
            # Interact with environment
            obs, reward, done, info = env.step(action)


            if info == "TimeOut":
                print("Step: ", stepIdx)

                print ("Throughput(Byte) :", throughput)
                print ("delay(μs) :", delay)

                if total_choice_counter != 0:
                    optimized_choice.append(optimized_choice_counter/total_choice_counter)

                throughput_list.append(throughput)
                delay_list.append(delay)
                enqueue_drop_list.append(enqueue_drop)
                cleanup_drop_list.append(cleanup_drop)
                break

            # Get free slot number
            free_slotNum = min(n_slotUsedTable - np.nonzero(obs[0])[0].size,MAXSLOTS)

            # Get queuing bytes
            queueBytes = obs[1][0]

            # Since there are multiple action in one step,
            # according to each action, it would have one reward.
            # But in ns3gym, the reward type is float, it means it can only return one reward,
            # we use the return info to send multiple reward, throughput, delay

            info_list = [float(r) for r in info.split(',')]
            reward_all = info_list[:3]
            throughput = info_list[3]
            delay = info_list[4]
            enqueue_drop = info_list[5]
            cleanup_drop = info_list[6]
            
            if throughput != 0:
                throughput_history.append(throughput)
            if delay != 0:
                delay_history.append(delay)
            if enqueue_drop != 0:
                enqueue_drop_history.append(enqueue_drop)
            if cleanup_drop != 0:
                cleanup_drop_history.append(cleanup_drop)
                
            #reward_all = [0,0,0]
            info = stepIdx

            #print("Step: ", stepIdx)
            #print("---obs, reward, done, info: ", obs, reward_all, done, info)

            # Change data type
            obs = np.array(list(obs[0]) + list(obs[1][1:]),dtype=float)
            # padding 0 if K < 3 (Top1_queuedBytes,Top2_queuedBytes) -> (Top1_queuedBytes,Top2_queuedBytes,0)
            obs = np.pad(obs,(0, ob_space_n - obs.size), constant_values = 0)
            # Normalize queuebytes
            obs[n_slotUsedTable:] = obs[n_slotUsedTable:]/queueBytes if queueBytes != 0 else np.zeros(3)

            top_index = []
            for i in range(3):
                # Get the slot used by top K next hop
                top_index.append(np.ravel(np.argwhere(_obs[:n_slotUsedTable]==i+1)))

            # Calculate r_i of each a_i
            for act, r in zip(action,reward_all):
                if act == -1:
                    continue


                total_choice_counter += 1
                nonLearn_data += 1

                # Only calculate reward of the non-collision action
                if (act not in np.nonzero(_obs[:n_slotUsedTable])[0]):
                    r += 0.2

                    #print ("act:",act)
                    #print ("r:",r)
                    #print ("throughput:",throughput)

                    isChosen = False
                    max_r = 0
                    # Add optimized reward
                    for i in range(3):
                        if any(act < idx for idx in top_index[i]):
                            distance_map = top_index[i] - act
                            distance = min(np.where(distance_map>0,distance_map,np.inf))
                            closest_distance = 0
                            for idx in range(closest_distance):
                                closest_distance = closest_distance + 1 if obs[act+idx] == 0 else closest_distance

                            distance_reward = (1.7-pow((closest_distance/10),0.2))
                            size_weight = _obs[-(3-i)]
                            max_r =  max((distance_reward*size_weight*0.9 + (throughput/max_throughput/2)*0.1),max_r) 

                            if not isChosen:
                                isChosen = True
                                optimized_choice_counter += 1

                    if all(idx.size == 0 for idx in top_index):
                        max_r = throughput/max_throughput/2



                if isTraining:
                    r += max_r
                    RL.store_transition(_obs, act, r, obs)



            #if stepIdx % 10000 == 0:
            #    print("Step: ", stepIdx)

            if (total_choice_counter > 5000) and (nonLearn_data > 10) and isTraining:
                nonLearn_data = 0
                RL.learn()

            # swap observation
            _obs = obs


            if done:
                print("Step: ", stepIdx)
                print ("done")

                print ("Throughput(Byte) :", throughput)
                print ("delay(μs) :", delay)
                print ("Enqueue Drop : ", enqueue_drop)
                print ("Cleanup Drop : ", cleanup_drop)
                
                if total_choice_counter != 0:
                    optimized_choice.append(optimized_choice_counter/total_choice_counter)

                throughput_list.append(throughput)
                delay_list.append(delay)
                enqueue_drop_list.append(enqueue_drop)
                cleanup_drop_list.append(cleanup_drop)
                
                break

            #break
    avg_throughput = sum(throughput_list)/len(throughput_list)
    avg_delay = sum(delay_list)/len(delay_list)
    avg_enqueue_drop = sum(enqueue_drop_list)/len(enqueue_drop_list)
    avg_cleanup_drop = sum(cleanup_drop_list)/len(cleanup_drop_list)
    
    print ("Avg Throughput: {}".format(avg_throughput))
    print ("Avg Delay: {}".format(avg_delay))
    print ("Avg Enqueue Drop: {}".format(avg_enqueue_drop))
    print ("Avg Cleanup Drop: {}".format(avg_cleanup_drop))
    
    throughput_by_pktNum.append(avg_throughput)
    delay_by_pktNum.append(avg_delay)
    enqueue_drop_by_pktNum.append(avg_enqueue_drop)
    cleanup_drop_by_pktNum.append(avg_cleanup_drop)
    env.close()
    
    # save weight to file
    eval_model.save_weights('model_weight/{}pkt/eval_model_weights'.format(pktNum))
    target_model.save_weights('model_weight/{}pkt/target_model_weights'.format(pktNum))

with open('/tf/log/result.csv', 'a') as f:
    s = ''
    for throughput in throughput_by_pktNum:
        s += str(throughput) + ','
    s = s[:-1]
    s += '\n'
    for delay in delay_by_pktNum:
        s += str(delay) + ','
    s = s[:-1]
    s += '\n'
    for enqueue_drop in enqueue_drop_by_pktNum:
        s += str(enqueue_drop) + ','
    s = s[:-1]
    s += '\n'
    for cleanup_drop in cleanup_drop_by_pktNum:
        s += str(cleanup_drop) + ','
    s = s[:-1]
    s += '\n'
    f.write('Experiment Finished at: {}'.format(time.time()))
    f.write(s)