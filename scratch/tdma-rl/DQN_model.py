import tensorflow as tf
import numpy as np
from tensorflow.python.keras import layers
from tensorflow.keras.optimizers import RMSprop


class Eval_Model(tf.keras.Model):
    def __init__(self, num_actions):
        super().__init__('mlp_q_network')
        self.layer1 = layers.Dense(128, activation='relu')
        self.layer2 = layers.Dense(128, activation='relu')
        self.logits = layers.Dense(num_actions, activation=None)

    def call(self, inputs):
        x = tf.convert_to_tensor(inputs)
        layer1 = self.layer1(x)
        layer2 = self.layer2(layer1)
        logits = self.logits(layer2)
        return logits


class Target_Model(tf.keras.Model):
    def __init__(self, num_actions):
        super().__init__('mlp_q_network_1')
        self.layer1 = layers.Dense(128, trainable=False, activation='relu')
        self.layer2 = layers.Dense(128, trainable=False, activation='relu')
        self.logits = layers.Dense(num_actions, trainable=False, activation=None)

    def call(self, inputs):
        x = tf.convert_to_tensor(inputs)
        layer1 = self.layer1(x)
        layer2 = self.layer2(layer1)
        logits = self.logits(layer2)
        return logits


class DeepQNetwork:
    def __init__(self, n_actions, n_max_chosen, n_states, eval_model, target_model, learning_rate, reward_decay, e_greedy, replace_target_iter, memory_size, batch_size):

        self.params = {
            'n_actions': n_actions,
            'n_max_chosen': n_max_chosen,
            'n_states': n_states,
            'learning_rate': learning_rate,
            'reward_decay': reward_decay,
            'e_greedy': e_greedy,
            'replace_target_iter': replace_target_iter,
            'memory_size': memory_size,
            'batch_size': batch_size,
            'e_greedy_increment': None
        }

        # total learning step

        self.learn_step_counter = 0

        # initialize zero memory [s, a, r, s_]
        self.epsilon = 0 if self.params['e_greedy_increment'] is not None else self.params['e_greedy']
        self.memory = np.zeros((self.params['memory_size'], self.params['n_states'] * 2 + 2))

        self.eval_model = eval_model
        self.target_model = target_model

        self.eval_model.compile(
            optimizer=RMSprop(lr=self.params['learning_rate']),
            loss='mse'
        )
        self.cost_his = []

    def store_transition(self, s, a, r, s_):
        if not hasattr(self, 'memory_counter'):
            self.memory_counter = 0

        transition = np.hstack((s, [a, r], s_))

        # replace the old memory with new memory
        index = self.memory_counter % self.params['memory_size']
        self.memory[index, :] = transition

        self.memory_counter += 1

    def choose_action(self, observation):
        # to have batch dimension when feed into tf placeholder
        observation = observation[np.newaxis, :]


        if np.random.uniform() < self.epsilon:
            # forward feed the observation and get q value for every actions
            actions_value = self.eval_model.predict(observation)[0]

            #action = np.argmax(actions_value)
            
            # return the top N action, and then using queuing bytes to decide 
            action_unsort = np.argpartition(actions_value,-self.params['n_max_chosen'])[-self.params['n_max_chosen']:]
            action = action_unsort[np.argsort(actions_value[action_unsort])]
        else:
            action = np.random.choice(self.params['n_actions'],self.params['n_max_chosen'],replace=False)
        return action

    def learn(self):

        # sample batch memory from all memory
        if self.memory_counter > self.params['memory_size']:
            sample_index = np.random.choice(self.params['memory_size'], size=self.params['batch_size'])
        else:
            sample_index = np.random.choice(self.memory_counter, size=self.params['batch_size'])

        batch_memory = self.memory[sample_index, :]

        q_next = self.target_model.predict(batch_memory[:, -self.params['n_states']:])
        q_eval = self.eval_model.predict(batch_memory[:, :self.params['n_states']])

        # change q_target w.r.t q_eval's action
        q_target = q_eval.copy()

        batch_index = np.arange(self.params['batch_size'], dtype=np.int32)
        eval_act_index = batch_memory[:, self.params['n_states']].astype(int)
        reward = batch_memory[:, self.params['n_states'] + 1]

        q_target[batch_index, eval_act_index] = reward + self.params['reward_decay'] * np.max(q_next, axis=1)

        # check to replace target parameters
        if self.learn_step_counter % self.params['replace_target_iter'] == 0:
            for eval_layer, target_layer in zip(self.eval_model.layers, self.target_model.layers):
                target_layer.set_weights(eval_layer.get_weights())
            print('\ntarget_params_replaced\n')

        """
        For example in this batch I have 2 samples and 3 actions:
        q_eval =
        [[1, 2, 3],
         [4, 5, 6]]
        q_target = q_eval =
        [[1, 2, 3],
         [4, 5, 6]]
        Then change q_target with the real q_target value w.r.t the q_eval's action.
        For example in:
            sample 0, I took action 0, and the max q_target value is -1;
            sample 1, I took action 2, and the max q_target value is -2:
        q_target =
        [[-1, 2, 3],
         [4, 5, -2]]
        So the (q_target - q_eval) becomes:
        [[(-1)-(1), 0, 0],
         [0, 0, (-2)-(6)]]
        We then backpropagate this error w.r.t the corresponding action to network,
        leave other action as error=0 cause we didn't choose it.
        """

        # train eval network

        self.cost = self.eval_model.train_on_batch(batch_memory[:, :self.params['n_states']], q_target)

        self.cost_his.append(self.cost)

        # increasing epsilon
        self.epsilon = self.epsilon + self.params['e_greedy_increment'] if self.epsilon < self.params['e_greedy'] \
            else self.params['e_greedy']
        self.learn_step_counter += 1

    def plot_cost(self):
        import matplotlib.pyplot as plt
        plt.plot(np.arange(len(self.cost_his)), self.cost_his)
        plt.ylabel('Cost')
        plt.xlabel('training steps')
        plt.show()
