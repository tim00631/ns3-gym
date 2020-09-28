ns3-gym
============

[OpenAI Gym](https://gym.openai.com/) is a toolkit for reinforcement learning (RL) widely used in research. The network simulator [ns–3](https://www.nsnam.org/) is the de-facto standard for academic and industry studies in the areas of networking protocols and communication technologies. ns3-gym is a framework that integrates both OpenAI Gym and ns-3 in order to encourage usage of RL in networking research.

Installation (ns3-gym)
============

1. Install all required dependencies required by ns-3.
```
# minimal requirements for C++:
apt-get install gcc g++ python

see https://www.nsnam.org/wiki/Installation
```
2. Install ZMQ and Protocol Buffers libs:
```
# to install protobuf-3.6 on ubuntu 16.04:
sudo add-apt-repository ppa:maarten-fonville/protobuf
sudo apt-get update

apt-get install libzmq5 libzmq5-dev
apt-get install libprotobuf-dev
apt-get install protobuf-compiler
```
3. Configure and build ns-3 project (if you are going to use Python virtual environment, please execute these commands inside it):
```
# Opengym Protocol Buffer messages (C++ and Python) are build during configure
./waf configure
./waf build
```

4. Install ns3gym located in src/opengym/model/ns3gym (Python3 required)
```
pip3 install ./src/opengym/model/ns3gym
```

5. (Optional) Install all libraries required by your agent (like tensorflow, keras, etc.).

6. Run example:
```
cd ./scratch/opengym
./simple_test.py
```

7. (Optional) Start ns-3 simulation script and Gym agent separately in two terminals (useful for debugging):
```
# Terminal 1
./waf --run "opengym"

# Terminal 2
cd ./scratch/opengym
./test.py --start=0
```

Setup (RL-TDMA)
============
1. Install tensorflow 2, we use official tensorflow2 docker instead (docker tag : tensorflow/tensorflow:nightly-gpu-py3-jupyter).
```
docker run --gpus all -it -p 8888:8888 --name rl -u $(id -u ${USER}):$(id -g ${USER}) -v $PWD:/workspace tensorflow/tensorflow:nightly-py3-jupyter
```

2. Follow the above Installation

3. Run ./ns3-gym/scratch/tdma-rl/training.ipynb



Examples
========

All examples can be found [here](./scratch/).

## Basic Interface

1. Example Python script. Note, that `gym.make('ns3-v0')` starts ns-3 simulation script located in current working directory.
```
import gym
import ns3gym
import MyAgent

env = gym.make('ns3-v0')
obs = env.reset()
agent = MyAgent.Agent()

while True:
  action = agent.get_action(obs)
  obs, reward, done, info = env.step(action)

  if done:
    break
env.close()
```
2. Any ns-3 simulation script can be used as a Gym environment. This requires only to instantiate OpenGymInterface and implement the ns3-gym C++ interface consisting of the following functions:
```
Ptr<OpenGymSpace> GetObservationSpace();
Ptr<OpenGymSpace> GetActionSpace();
Ptr<OpenGymDataContainer> GetObservation();
float GetReward();
bool GetGameOver();
std::string GetExtraInfo();
bool ExecuteActions(Ptr<OpenGymDataContainer> action);
```
Note, that the generic ns3-gym interface allows to observe any variable or parameter in a simulation.

A more detailed description can be found in our [Paper](http://www.tkn.tu-berlin.de/fileadmin/fg112/Papers/2019/gawlowicz19_mswim.pdf).






How to reference ns3-gym?
============

Please use the following bibtex :

```
@inproceedings{ns3gym,
  Title = {{ns-3 meets OpenAI Gym: The Playground for Machine Learning in Networking Research}},
  Author = {Gaw{\l}owicz, Piotr and Zubow, Anatolij},
  Booktitle = {{ACM International Conference on Modeling, Analysis and Simulation of Wireless and Mobile Systems (MSWiM)}},
  Year = {2019},
  Location = {Miami Beach, USA},
  Month = {November},
  Url = {http://www.tkn.tu-berlin.de/fileadmin/fg112/Papers/2019/gawlowicz19_mswim.pdf}
}
```
