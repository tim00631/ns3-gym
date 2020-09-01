import tensorflow as tf
import numpy as np
from tensorflow.python.keras import layers
from tensorflow.keras.optimizers import RMSprop


class SumTree(object):
    """
    This SumTree code is a modified version and the original code is from:
    https://github.com/jaara/AI-blog/blob/master/SumTree.py
    Story data with its priority in the tree.
    """
    data_pointer = 0

    def __init__(self, capacity):
        self.capacity = capacity  # for all priority values
        self.tree = np.zeros(2 * capacity - 1)
        # [--------------Parent nodes-------------][-------leaves to recode priority-------]
        #             size: capacity - 1                       size: capacity
        self.data = np.zeros(capacity, dtype=object)  # for all transitions
        # [--------------data frame-------------]
        #             size: capacity

    def add(self, p, data):
        tree_idx = self.data_pointer + self.capacity - 1
        self.data[self.data_pointer] = data  # update data_frame
        self.update(tree_idx, p)  # update tree_frame

        self.data_pointer += 1
        if self.data_pointer >= self.capacity:  # replace when exceed the capacity
            self.data_pointer = 0

    def update(self, tree_idx, p):
        change = p - self.tree[tree_idx]
        self.tree[tree_idx] = p
        # then propagate the change through tree
        while tree_idx != 0:    # this method is faster than the recursive loop in the reference code
            tree_idx = (tree_idx - 1) // 2
            self.tree[tree_idx] += change

    def get_leaf(self, v):
        """
        Tree structure and array storage:
        Tree index:
             0         -> storing priority sum
            / \
          1     2
         / \   / \
        3   4 5   6    -> storing priority for transitions
        Array type for storing:
        [0,1,2,3,4,5,6]
        """
        parent_idx = 0
        while True:     # the while loop is faster than the method in the reference code
            cl_idx = 2 * parent_idx + 1         # this leaf's left and right kids
            cr_idx = cl_idx + 1
            if cl_idx >= len(self.tree):        # reach bottom, end search
                leaf_idx = parent_idx
                break
            else:       # downward search, always search for a higher priority node
                if v <= self.tree[cl_idx]:
                    parent_idx = cl_idx
                else:
                    v -= self.tree[cl_idx]
                    parent_idx = cr_idx

        data_idx = leaf_idx - self.capacity + 1
        return leaf_idx, self.tree[leaf_idx], self.data[data_idx]

    @property
    def total_p(self):
        return self.tree[0]  # the root


class Memory(object):  # stored as ( s, a, r, s_ ) in SumTree
    """
    This Memory class is modified based on the original code from:
    https://github.com/jaara/AI-blog/blob/master/Seaquest-DDQN-PER.py
    """
    epsilon = 0.01  # small amount to avoid zero priority
    alpha = 0.6  # [0~1] convert the importance of TD error to priority
    beta = 0.4  # importance-sampling, from initial value increasing to 1
    beta_increment_per_sampling = 0.001
    abs_err_upper = 1.  # clipped abs error

    def __init__(self, capacity):
        self.tree = SumTree(capacity)

    def store(self, transition):
        max_p = np.max(self.tree.tree[-self.tree.capacity:])
        if max_p == 0:
            max_p = self.abs_err_upper
        self.tree.add(max_p, transition)   # set the max p for new p

    def sample(self, n):
        b_idx, b_memory, ISWeights = np.empty((n,), dtype=np.int32), np.empty((n, self.tree.data[0].size)), np.empty((n, 1))
        pri_seg = self.tree.total_p / n       # priority segment
        self.beta = np.min([1., self.beta + self.beta_increment_per_sampling])  # max = 1

        min_prob = np.min(self.tree.tree[-self.tree.capacity:]) / self.tree.total_p     # for later calculate ISweight
        for i in range(n):
            a, b = pri_seg * i, pri_seg * (i + 1)
            v = np.random.uniform(a, b)
            idx, p, data = self.tree.get_leaf(v)
            prob = p / self.tree.total_p
            ISWeights[i, 0] = np.power(prob/min_prob, -self.beta)
            b_idx[i], b_memory[i, :] = idx, data
        return b_idx, b_memory, ISWeights

    def batch_update(self, tree_idx, abs_errors):
        abs_errors += self.epsilon  # convert to abs and avoid 0
        clipped_errors = np.minimum(abs_errors, self.abs_err_upper)
        ps = np.power(clipped_errors, self.alpha)
        for ti, p in zip(tree_idx, ps):
            self.tree.update(ti, p)

class Eval_Model(tf.keras.Model):
    def __init__(self, num_actions):
        super().__init__('mlp_q_network')
        self.input_layer = tf.keras.layers.InputLayer(input_shape=(35,),dtype='float32')
        self.hidden_layers = layers.Dense(35,kernel_initializer='RandomNormal')
        self.logits = layers.Dense(num_actions, activation=None)

    def call(self, inputs):
        x = self.input_layer(inputs)
        x = self.hidden_layers(x)
        x = tf.keras.layers.LeakyReLU(alpha=0.3)(x)
        logits = self.logits(x)
        return logits


class Target_Model(tf.keras.Model):
    def __init__(self, num_actions):
        super().__init__('mlp_q_network_1')
        self.input_layer = tf.keras.layers.InputLayer(input_shape=(35,),dtype='float32')
        self.hidden_layers = layers.Dense(35, trainable=False)
        self.logits = layers.Dense(num_actions, trainable=False, activation=None)

    def call(self, inputs):
        x = self.input_layer(inputs)
        x = self.hidden_layers(x)
        x = tf.keras.layers.LeakyReLU(alpha=0.3)(x)
        logits = self.logits(x)
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
        #self.memory = np.zeros((self.params['memory_size'], self.params['n_states'] * 2 + 2))
        
        # initalize prioritized memory
        self.memory = Memory(capacity=memory_size)

        self.eval_model = eval_model
        self.target_model = target_model

        self.eval_model.compile(
            optimizer=RMSprop(lr=self.params['learning_rate']),
            #loss='mse'
            loss=self._per_loss
        )
        self.cost_his = []
        
        
        # prioritized memory weight
        self.beta = 0.4
        self.is_weight = np.power(memory_size, -self.beta)  # because p1 == 1
        
        
    def _per_loss(self, y_target, y_pred):
        return tf.reduce_mean(self.is_weight * tf.math.squared_difference(y_target, y_pred))
        
    def store_transition(self, s, a, r, s_):
        """
        if not hasattr(self, 'memory_counter'):
            self.memory_counter = 0

        transition = np.hstack((s, [a, r], s_))

        # replace the old memory with new memory
        index = self.memory_counter % self.params['memory_size']
        self.memory[index, :] = transition

        self.memory_counter += 1
        """
        
        # Store in transition
        transition = np.hstack((s, [a, r], s_))
        self.memory.store(transition)    # have high priority for newly arrived transition
        
        
    def choose_action(self, observation, isTraining=True):
        # to have batch dimension when feed into tf placeholder
        observation = observation[np.newaxis, :]


        if np.random.uniform() < self.epsilon or not isTraining:
            # forward feed the observation and get q value for every actions
            actions_value = self.eval_model.predict(observation)[0]
            
            # Original: output Top 1 q_value's index
            #action = np.argmax(actions_value)
            
            # return the top N action, and then using queuing bytes to decide 
            #action_unsort = np.argpartition(actions_value,-self.params['n_max_chosen'])[-self.params['n_max_chosen']:]
            #action = action_unsort[np.argsort(actions_value[action_unsort])]
        else:
            #action = np.random.choice(self.params['n_actions'],self.params['n_max_chosen'],replace=False)
            actions_value = np.random.random_sample((self.params['n_actions'],))
        return actions_value

    def learn(self):
        
        """
        # sample batch memory from all memory
        if self.memory_counter > self.params['memory_size']:
            sample_index = np.random.choice(self.params['memory_size'], size=self.params['batch_size'])
        else:
            sample_index = np.random.choice(self.memory_counter, size=self.params['batch_size'])

        batch_memory = self.memory[sample_index, :]
        """
        # Sample from sumTree memory
        tree_idx, batch_memory, self.is_weight = self.memory.sample(self.params['batch_size'])
        
        q_next = self.target_model.predict(batch_memory[:, -self.params['n_states']:])[0]
        #q_next = self.target_model.predict(batch_memory[:, -self.params['n_states']:])
        q_eval = self.eval_model.predict(batch_memory[:, :self.params['n_states']])
        
        q_index = self.eval_model.predict(batch_memory[:, -self.params['n_states']:])[0]
        q_index = np.argmax(q_index)
        
        
        # change q_target w.r.t q_eval's action
        q_target = q_eval.copy()

        batch_index = np.arange(self.params['batch_size'], dtype=np.int32)
        eval_act_index = batch_memory[:, self.params['n_states']].astype(int)
        reward = batch_memory[:, self.params['n_states'] + 1]

        # using the eval_net argmax as index to get the target_net max reward
        #q_target[batch_index, eval_act_index] = reward + self.params['reward_decay'] * np.max(q_next, axis=1)
        q_target[batch_index, eval_act_index] = reward + self.params['reward_decay'] * q_next[q_index]
        
        
        # check to replace target parameters
        if self.learn_step_counter % self.params['replace_target_iter'] == 0:
            for eval_layer, target_layer in zip(self.eval_model.layers, self.target_model.layers):
                target_layer.set_weights(eval_layer.get_weights())
            #print('\ntarget_params_replaced')

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

        # Update prioritized memory
        abs_errors = tf.reduce_sum(tf.abs(q_target - q_eval), axis=1)    # for updating Sumtree
        self.memory.batch_update(tree_idx, abs_errors)
        
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
