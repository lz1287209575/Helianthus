#include "../../Shared/Database/ORM.h"

namespace Helianthus::Database::ORM {

QueryBuilder& QueryBuilder::Select(const std::vector<std::string>& Columns) {
    Type = QueryType::SELECT;
    SelectColumns = Columns;
    return *this;
}

QueryBuilder& QueryBuilder::From(const std::string& Table) {
    TableName = Table;
    return *this;
}

QueryBuilder& QueryBuilder::Where(const std::string& Condition) {
    WhereConditions.push_back(Condition);
    return *this;
}

QueryBuilder& QueryBuilder::WhereEquals(const std::string& Column, const DatabaseValue& /*Value*/) {
    WhereConditions.push_back(Column + " = ?");
    return *this;
}

QueryBuilder& QueryBuilder::WhereIn(const std::string& Column, const std::vector<DatabaseValue>& Values) {
    (void)Values;
    WhereConditions.push_back(Column + " IN (?)");
    return *this;
}

QueryBuilder& QueryBuilder::WhereBetween(const std::string& Column, const DatabaseValue& /*Start*/, const DatabaseValue& /*End*/) {
    WhereConditions.push_back(Column + " BETWEEN ? AND ?");
    return *this;
}

QueryBuilder& QueryBuilder::OrderBy(const std::string& Column, bool Ascending) {
    OrderByColumns.push_back(Column + std::string(Ascending ? " ASC" : " DESC"));
    return *this;
}

QueryBuilder& QueryBuilder::GroupBy(const std::string& Column) {
    GroupByColumns.push_back(Column);
    return *this;
}

QueryBuilder& QueryBuilder::Having(const std::string& Condition) {
    HavingCondition = Condition;
    return *this;
}

QueryBuilder& QueryBuilder::Limit(uint32_t Count) {
    LimitCount = Count;
    return *this;
}

QueryBuilder& QueryBuilder::Offset(uint32_t Count) {
    OffsetCount = Count;
    return *this;
}

QueryBuilder& QueryBuilder::Join(const std::string& Table, const std::string& Condition) {
    JoinClauses.push_back("JOIN " + Table + " ON " + Condition);
    return *this;
}

QueryBuilder& QueryBuilder::LeftJoin(const std::string& Table, const std::string& Condition) {
    JoinClauses.push_back("LEFT JOIN " + Table + " ON " + Condition);
    return *this;
}

QueryBuilder& QueryBuilder::RightJoin(const std::string& Table, const std::string& Condition) {
    JoinClauses.push_back("RIGHT JOIN " + Table + " ON " + Condition);
    return *this;
}

QueryBuilder& QueryBuilder::InnerJoin(const std::string& Table, const std::string& Condition) {
    JoinClauses.push_back("INNER JOIN " + Table + " ON " + Condition);
    return *this;
}

QueryBuilder& QueryBuilder::InsertInto(const std::string& Table) {
    Type = QueryType::INSERT;
    TableName = Table;
    return *this;
}

QueryBuilder& QueryBuilder::Values(const ParameterMap& Values) {
    UpdateValues = Values;
    return *this;
}

QueryBuilder& QueryBuilder::Update(const std::string& Table) {
    Type = QueryType::UPDATE;
    TableName = Table;
    return *this;
}

QueryBuilder& QueryBuilder::Set(const std::string& Column, const DatabaseValue& Value) {
    UpdateValues[Column] = Value;
    return *this;
}

QueryBuilder& QueryBuilder::Set(const ParameterMap& Values) {
    for (const auto& [k, v] : Values) UpdateValues[k] = v;
    return *this;
}

QueryBuilder& QueryBuilder::DeleteFrom(const std::string& Table) {
    Type = QueryType::DELETE;
    TableName = Table;
    return *this;
}

std::string QueryBuilder::Build() const {
    switch (Type) {
        case QueryType::SELECT: return BuildSelect();
        case QueryType::INSERT: return BuildInsert();
        case QueryType::UPDATE: return BuildUpdate();
        case QueryType::DELETE: return BuildDelete();
    }
    return {};
}

ParameterMap QueryBuilder::GetParameters() const { return Parameters; }

void QueryBuilder::Clear() {
    SelectColumns.clear();
    TableName.clear();
    WhereConditions.clear();
    JoinClauses.clear();
    OrderByColumns.clear();
    GroupByColumns.clear();
    HavingCondition.clear();
    Parameters.clear();
    UpdateValues.clear();
    LimitCount = 0;
    OffsetCount = 0;
}

std::string QueryBuilder::BuildSelect() const {
    std::ostringstream oss;
    oss << "SELECT";
    if (!SelectColumns.empty()) {
        oss << " ";
        for (size_t i = 0; i < SelectColumns.size(); ++i) {
            if (i) oss << ",";
            oss << SelectColumns[i];
        }
    } else {
        oss << " *";
    }
    if (!TableName.empty()) oss << " FROM " << TableName;
    if (!JoinClauses.empty()) {
        oss << " ";
        for (size_t i = 0; i < JoinClauses.size(); ++i) {
            if (i) oss << " ";
            oss << JoinClauses[i];
        }
    }
    if (!WhereConditions.empty()) {
        oss << " WHERE ";
        for (size_t i = 0; i < WhereConditions.size(); ++i) {
            if (i) oss << " AND ";
            oss << WhereConditions[i];
        }
    }
    if (!GroupByColumns.empty()) {
        oss << " GROUP BY ";
        for (size_t i = 0; i < GroupByColumns.size(); ++i) {
            if (i) oss << ",";
            oss << GroupByColumns[i];
        }
    }
    if (!HavingCondition.empty()) {
        oss << " HAVING " << HavingCondition;
    }
    if (!OrderByColumns.empty()) {
        oss << " ORDER BY ";
        for (size_t i = 0; i < OrderByColumns.size(); ++i) {
            if (i) oss << ",";
            oss << OrderByColumns[i];
        }
    }
    if (LimitCount) oss << " LIMIT " << LimitCount;
    if (OffsetCount) oss << " OFFSET " << OffsetCount;
    return oss.str();
}

std::string QueryBuilder::BuildInsert() const {
    std::ostringstream oss;
    oss << "INSERT INTO " << TableName << " VALUES(...)";
    return oss.str();
}

std::string QueryBuilder::BuildUpdate() const {
    std::ostringstream oss;
    oss << "UPDATE " << TableName << " SET ...";
    if (!WhereConditions.empty()) {
        oss << " WHERE ";
        for (size_t i = 0; i < WhereConditions.size(); ++i) {
            if (i) oss << " AND ";
            oss << WhereConditions[i];
        }
    }
    return oss.str();
}

std::string QueryBuilder::BuildDelete() const {
    std::ostringstream oss;
    oss << "DELETE FROM " << TableName;
    if (!WhereConditions.empty()) {
        oss << " WHERE ";
        for (size_t i = 0; i < WhereConditions.size(); ++i) {
            if (i) oss << " AND ";
            oss << WhereConditions[i];
        }
    }
    return oss.str();
}

} // namespace Helianthus::Database::ORM


