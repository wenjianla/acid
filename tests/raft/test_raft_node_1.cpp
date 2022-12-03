//
// Created by zavier on 2022/6/29.
//

#include "acid/raft/raft_node.h"

using namespace acid;
using namespace acid::raft;

std::map<int64_t, std::string> peers = {
        {1, "localhost:7001"},
        {2, "localhost:7002"},
        {3, "localhost:7003"},
};

void Main() {
    int64_t id = 1;
    Persister::ptr persister = std::make_shared<Persister>(fmt::format("raft-node-{}", id));
    co::co_chan<ApplyMsg> applyChan;
    RaftNode node(id, persister, applyChan);
    Address::ptr addr = Address::LookupAny(peers[node.getNodeId()]);
    node.bind(addr);
    for (auto peer: peers) {
        if (peer.first == node.getNodeId())
            continue;
        Address::ptr address = Address::LookupAny(peer.second);
        // 添加节点
        node.addPeer(peer.first, address);
    }

    go [&node, id] {
        for (int i = 0; ; ++i) {
            if (node.isLeader()) {
                node.propose(fmt::format("Node[{}] propose {}", id, i));
            }
            sleep(5);
        }
    };
    go [applyChan, id, &node] {
        // 接收raft达成共识的日志
        ApplyMsg msg;
        while (applyChan.pop(msg)) {
            if (msg.type == ApplyMsg::ENTRY && msg.index % 10 == 0 && node.isLeader()) {
                // 十条日志做一次快照
                node.persistStateAndSnapshot(msg.index, fmt::format("Node[{}] create snapshot, index {}", id, msg.index));
            }
            switch (msg.type) {
                case ApplyMsg::ENTRY:
                    SPDLOG_INFO("entry-> index: {}, term: {}, data: {}", msg.index, msg.term, msg.data);
                    break;
                case ApplyMsg::SNAPSHOT:
                    SPDLOG_INFO("snapshot-> index: {}, term: {}, data: {}", msg.index, msg.term, msg.data);
                    break;
            }
        }
    };
    // 启动raft节点
    node.start();
}

int main() {
    go Main;
    co_sched.Start();
}