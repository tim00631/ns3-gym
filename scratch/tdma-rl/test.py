#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import gym
import argparse
from ns3gym import ns3env

port = 0
simTime = 20 # seconds
stepTime = 0.5  # seconds
seed = 0
simArgs = {"--simTime": simTime,
           "--testArg": 123}

env = gym.make('ns3-v0')
#env = ns3env.Ns3Env(debug=True)

env.reset()



ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)

stepIdx = 0

try:
    obs = env.reset()
    print("Step: ", stepIdx)
    print("---obs: ", obs)

    while True:
        stepIdx += 1

        action = env.action_space.sample()
        print("---action: ", action)
        obs, reward, done, info = env.step(action)

        print("Step: ", stepIdx)
        print("---obs, reward, done, info: ", obs, reward, done, info)
        

        if done:
            break

        if stepIdx == 100 :
            break

except KeyboardInterrupt:
    print("Ctrl-C -> Exit")
finally:
    env.close()
    print("Done")